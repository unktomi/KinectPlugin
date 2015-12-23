#include "KinectPluginPrivatePCH.h"
#include "KinectFusionActor.h"
#include "AllowWindowsPlatformTypes.h"
#include "NuiKinectFusionApi.h"
#include "KinectFusionProcessor.h"
#include "comdef.h"

static void LogKinectError(const FString &context, int hr) {
	_com_error err(hr);
	LPCTSTR errMsg = err.ErrorMessage();
	UE_LOG(LogKinect, Error, TEXT("%s: %d: %s"), *context, hr, errMsg);
}


AKinectFusionActor::AKinectFusionActor(const class FObjectInitializer& PCIP)
: Super(PCIP)
, EnablePhysics(false)
, Processor(0)
, Camera(0)
, DepthCamera(0)
, InfraredCamera(0),
VoxelsPerMeter(256/2),
VoxelCountX(384),
VoxelCountY(384),
VoxelCountZ(384),
LastReconstruction(10.0f),
AutoResetReconstructionWhenLost(true),
AutoResetReconstructionOnTimeout(true), // We now try to find the camera pose, however, setting this false will no longer auto reset on .xef file playback
AutoFindCameraPoseWhenLost(true),
MinDepthThreshold(NUI_FUSION_DEFAULT_MINIMUM_DEPTH),
MaxDepthThreshold(NUI_FUSION_DEFAULT_MAXIMUM_DEPTH),
MirrorDepthFrame(false),
MaxIntegrationWeight(NUI_FUSION_DEFAULT_INTEGRATION_WEIGHT),
DisplaySurfaceNormals(false),
CaptureColor(true),
ColorIntegrationInterval(3),
TranslateResetPoseByMinDepthThreshold(true),
DeltaFromReferenceFrameCalculationInterval(2),
MinSuccessfulTrackingFramesForCameraPoseFinder(45), // only update the camera pose finder initially after 45 successful frames (1.5s)
MinSuccessfulTrackingFramesForCameraPoseFinderAfterFailure(200), // resume integration following 200 successful frames after tracking failure (~7s)
MaxCameraPoseFinderPoseHistory(NUI_FUSION_CAMERA_POSE_FINDER_DEFAULT_POSE_HISTORY_COUNT),
CameraPoseFinderFeatureSampleLocationsPerFrame(NUI_FUSION_CAMERA_POSE_FINDER_DEFAULT_FEATURE_LOCATIONS_PER_FRAME_COUNT),
MaxCameraPoseFinderDepthThreshold(NUI_FUSION_CAMERA_POSE_FINDER_DEFAULT_MAX_DEPTH_THRESHOLD),
CameraPoseFinderDistanceThresholdReject(1.0f), // a value of 1.0 means no rejection
CameraPoseFinderDistanceThresholdAccept(0.1f),
MaxCameraPoseFinderPoseTests(5),
CameraPoseFinderProcessFrameCalculationInterval(5),
MaxAlignToReconstructionEnergyForSuccess(0.27f),
MinAlignToReconstructionEnergyForSuccess(0.005f),
MaxAlignPointCloudsEnergyForSuccess(0.006f),
MinAlignPointCloudsEnergyForSuccess(0.0f),
SmoothingKernelWidth(1),                 // 0=just copy, 1=3x3, 2=5x5, 3=7x7, here we create a 3x3 kernel
SmoothingDistanceThreshold(0.04f),       // 4cm, could use up to around 0.1f
AlignPointCloudsImageDownsampleFactor(2),// 1 = no down sample (process at epthImageResolution), 2=x/2,y/2, 4=x/4,y/4
MaxTranslationDelta(0.3f),               // 0.15 - 0.3m per frame typical
MaxRotationDelta(20.0f),
Thread(nullptr)
{
  MeshComp = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
  PrimaryActorTick.bCanEverTick = true;
}

AKinectFusionActor::~AKinectFusionActor()
{
  delete Processor;
}

void AKinectFusionActor::BeginPlay()
{
	if (Processor == nullptr)
	{
		Processor = new KinectFusionProcessor();
	}
	KinectFusionParams Params;

	Params.m_bAutoResetReconstructionWhenLost = AutoResetReconstructionWhenLost;
	Params.m_bAutoResetReconstructionOnTimeout = AutoResetReconstructionOnTimeout; // We now try to find the camera pose, however, setting this false will no longer auto reset on .xef file playback
	Params.m_bAutoFindCameraPoseWhenLost = AutoFindCameraPoseWhenLost;
	Params.m_fMinDepthThreshold = MinDepthThreshold;
	Params.m_fMaxDepthThreshold = MaxDepthThreshold;
	Params.m_bMirrorDepthFrame = MirrorDepthFrame;
	Params.m_cMaxIntegrationWeight = MaxIntegrationWeight;
	Params.m_bDisplaySurfaceNormals = DisplaySurfaceNormals;
	Params.m_bCaptureColor = CaptureColor;
	Params.m_cColorIntegrationInterval = ColorIntegrationInterval;
	Params.m_bTranslateResetPoseByMinDepthThreshold = TranslateResetPoseByMinDepthThreshold;
	Params.m_cDeltaFromReferenceFrameCalculationInterval = DeltaFromReferenceFrameCalculationInterval;
	Params.m_cMinSuccessfulTrackingFramesForCameraPoseFinder = MinSuccessfulTrackingFramesForCameraPoseFinder; // only update the camera pose finder initially after 45 successful frames (1.5s)
	Params.m_cMinSuccessfulTrackingFramesForCameraPoseFinderAfterFailure = MinSuccessfulTrackingFramesForCameraPoseFinderAfterFailure; // resume integration following 200 successful frames after tracking failure (~7s)
	Params.m_cMaxCameraPoseFinderPoseHistory = MaxCameraPoseFinderPoseHistory;
	Params.m_cCameraPoseFinderFeatureSampleLocationsPerFrame = CameraPoseFinderFeatureSampleLocationsPerFrame;
	Params.m_fMaxCameraPoseFinderDepthThreshold = MaxCameraPoseFinderDepthThreshold;
	Params.m_fCameraPoseFinderDistanceThresholdReject = CameraPoseFinderDistanceThresholdReject; // a value of 1.0 means no rejection
	Params.m_fCameraPoseFinderDistanceThresholdAccept = CameraPoseFinderDistanceThresholdAccept;
	Params.m_cMaxCameraPoseFinderPoseTests = MaxCameraPoseFinderPoseTests;
	Params.m_cCameraPoseFinderProcessFrameCalculationInterval = CameraPoseFinderProcessFrameCalculationInterval;
	Params.m_fMaxAlignToReconstructionEnergyForSuccess = MaxAlignToReconstructionEnergyForSuccess;
	Params.m_fMinAlignToReconstructionEnergyForSuccess = MinAlignToReconstructionEnergyForSuccess;
	Params.m_fMaxAlignPointCloudsEnergyForSuccess = MaxAlignPointCloudsEnergyForSuccess;
	Params.m_fMinAlignPointCloudsEnergyForSuccess = MinAlignPointCloudsEnergyForSuccess;
	Params.m_cSmoothingKernelWidth = SmoothingKernelWidth;                 // 0=just copy, 1=3x3, 2=5x5, 3=7x7, here we create a 3x3 kernel
	Params.m_fSmoothingDistanceThreshold = SmoothingDistanceThreshold;       // 4cm, could use up to around 0.1f
	Params.m_cAlignPointCloudsImageDownsampleFactor = AlignPointCloudsImageDownsampleFactor; // 1 = no down sample  = AlignPointCloudsImageDownsampleFactor(2),// 1 = no down sample ; 2=x/2,y/2, 4=x/4,y/4
	Params.m_fMaxTranslationDelta = MaxTranslationDelta;               // 0.15 - 0.3m per frame typical
	Params.m_fMaxRotationDelta = MaxRotationDelta;                  // 10-20 degrees per frame typical
	Params.m_reconstructionParams.voxelsPerMeter = VoxelsPerMeter;
	Params.m_reconstructionParams.voxelCountX = VoxelCountX;
	Params.m_reconstructionParams.voxelCountY = VoxelCountY;
	Params.m_reconstructionParams.voxelCountZ = VoxelCountZ;
	
	Processor->SetParams(Params);
	Processor->StartProcessing();
	if (Thread != nullptr)
	{
		delete Thread;
	}
	Thread = FRunnableThread::Create(this, TEXT("MeshGenerator"));
}


void AKinectFusionActor::Tick(float DeltaTime)
{	
	Update();
}

uint32 AKinectFusionActor::Run()
{
	const int proc = PlayCount;
	while (PlayCount == proc)
	{
		INuiFusionColorMesh *mesh = nullptr;
		HRESULT hr = Processor->CalculateMesh(&mesh);
		if (FAILED(hr))
		{
			if (hr != E_FAIL) {
				LogKinectError("CalculateMesh", hr);
			}
			continue;
		};		
		UpdateVertexData(mesh);
		MeshGeneration.Enqueue(mesh);
		FPlatformProcess::Sleep(0.16f);
	}
	return 0;
}

int AKinectFusionActor::Update()
{
	struct INuiFusionColorMesh *Mesh = 0;
	while (MeshGeneration.Dequeue(Mesh)) 
	{
		Mesh->Release();
		this->MeshComp->ClearAllMeshSections();
		this->MeshComp->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, EnablePhysics);
	}
	return 0;
}

int AKinectFusionActor::UpdateVertexData(INuiFusionColorMesh *mesh)
{
	unsigned int numVertices = mesh->VertexCount();
	unsigned int numTriangleIndices = mesh->TriangleVertexIndexCount();
	unsigned int numTriangles = numVertices / 3;

	if (0 == numVertices || 0 == numTriangleIndices || 0 != numVertices % 3 || numVertices != numTriangleIndices)
	{
		return 0;
	}

	const Vector3 *vertices = NULL;
	HRESULT hr = mesh->GetVertices(&vertices);
	if (FAILED(hr))
	{
		return hr;
	}
	Vertices.Empty();
	for (unsigned int i = 0; i < numVertices; i++)
	{
		const Vector3 &v = vertices[i];
		Vertices.Add(FVector(v.z, -v.x, -v.y) * 100.0f);
	}

	const Vector3 *normals = NULL;
	hr = mesh->GetNormals(&normals);
	if (FAILED(hr))
	{
		return hr;
	}
	Normals.Empty();
	for (unsigned int i = 0; i < numVertices; i++)
	{
		const Vector3 &v = normals[i];
		Normals.Add(FVector(v.z, -v.x, -v.y).GetSafeNormal());
	}
	const int *triangleIndices = NULL;
	hr = mesh->GetTriangleIndices(&triangleIndices);
	if (FAILED(hr))
	{
		return hr;
	}
	Triangles.Empty();
	for (unsigned int i = 0; i < numTriangleIndices; i++)
	{
		const int index = triangleIndices[i];
		Triangles.Add(index);
	}
	int const *colors;
	hr = mesh->GetColors(&colors);
	if (FAILED(hr))
	{
		return hr;
	}
	VertexColors.Empty();
	for (unsigned int i = 0; i < numVertices; i++)
	{
		int color = colors[i];
		VertexColors.Add(FColor(color));
	}
	return S_OK;
}
 

void AKinectFusionActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Processor->StopProcessing();
	PlayCount++;
	Thread->WaitForCompletion();
	delete Thread;
	Thread = nullptr;
}
