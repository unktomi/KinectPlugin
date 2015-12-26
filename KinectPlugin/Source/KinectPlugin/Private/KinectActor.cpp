#include "KinectPluginPrivatePCH.h"
#include "KinectActor.h"
#include "Vector2D.h"
#include "AllowWindowsPlatformTypes.h"
#include "comdef.h"
#include "ppl.h"

static void LogKinectError(const FString &context, int hr) {
	_com_error err(hr);
	LPCTSTR errMsg = err.ErrorMessage();
	UE_LOG(LogKinect, Error, TEXT("%s: %d: %s"), *context, hr, errMsg);
}


AKinectActor::AKinectActor(const class FObjectInitializer& PCIP)
	: Super(PCIP)
	, EnablePhysics(false)
	, Camera(0)
	, DepthCamera(0)
	, InfraredCamera(0)
	, ColorSource(0)
	, DepthSource(0)
	, BodySource(0)
	, BodyIndexSource(0)
	, ColorReader(0)
	, DepthReader(0)
	, BodyReader(0)
	, BodyIndexReader(0)
	, bEnableBodyIndexMask(false)
	, CoordinateMapper(0)
	, Resolution(2)
	, MaxEdgeLength(8)
	, InnerBandThreshold(2)
	, OuterBandThreshold(5)
	, bEnableDepthSmoothing(false)
	, ViewportWidth(1.0f)
	, ViewportHeight(1.0f)
	, MinDistanceInMeters(0)
	, MaxDistanceInMeters(2)
	, BilateralFilterKernelSize(4)
	, bPlaying(false)
	, Thread(nullptr)
	, HoleFillingRadius(10)
	, SmoothingRadius(2)
	, Tc(2)
	, Te(2)
	, Tr(10)

{
	MeshComp = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	PrimaryActorTick.bCanEverTick = true;
	BodyIndexMask.SetNumZeroed(BODY_COUNT);
}

AKinectActor::~AKinectActor()
{

}

#define SafeRelease(p) { if (p) p->Release(); p = nullptr; }

void AKinectActor::BeginPlay()
{
	Camera = NewObject<UKinectTexture>();
	HRESULT hResult = S_OK;
	hResult = GetDefaultKinectSensor(&Sensor);
	if (FAILED(hResult)){
		return;
	}

	// Open Sensor
	hResult = Sensor->Open();
	if (FAILED(hResult))
	{
		LogKinectError("Sensor Open", hResult);
		return;
	}

	// Retrieved Coordinate Mapper
	hResult = Sensor->get_CoordinateMapper(&CoordinateMapper);
	if (FAILED(hResult))
	{
		LogKinectError("Get Coordinate Mapper", hResult);
		return;
	}

	// Retrieved Color Frame Source
	hResult = Sensor->get_ColorFrameSource(&ColorSource);
	if (FAILED(hResult)){
		LogKinectError("Get Color Frame Source", hResult);
		return;
	}

	// Retrieved Depth Frame Source
	hResult = Sensor->get_DepthFrameSource(&DepthSource);
	if (FAILED(hResult))
	{
		LogKinectError("Get Depth Frame Source", hResult);
		return;
	}

	// Open Color Frame Reader
	hResult = ColorSource->OpenReader(&ColorReader);
	if (FAILED(hResult))
	{
		LogKinectError("Color Source Open Reader", hResult);
		return;
	}

	// Open Depth Frame Reader
	hResult = DepthSource->OpenReader(&DepthReader);
	if (FAILED(hResult))
	{
		LogKinectError("Depth Source Open Reader", hResult);
		return;
	}

	hResult = Sensor->get_BodyFrameSource(&BodySource);
	if (FAILED(hResult)){
		LogKinectError("Error : IKinectSensor::get_BodyFrameSource()", hResult);
		return;
	}

	hResult = BodySource->OpenReader(&BodyReader);
	if (FAILED(hResult)){
		LogKinectError("Error : IBodyFrameSource::OpenReader()", hResult);
		return;
	}
	
	// Retrieved Color Frame Size
	IFrameDescription* pColorDescription;
	hResult = ColorSource->get_FrameDescription(&pColorDescription);
	if (FAILED(hResult))
	{
		LogKinectError("Color Source Frame Description", hResult);
		return;
	}
	pColorDescription->get_Width(&ColorWidth); // 1920
	pColorDescription->get_Height(&ColorHeight); // 1080
	pColorDescription->Release();
	if (Camera) Camera->SetDimensions(FIntPoint(ColorWidth, ColorHeight));

	// To Reserve Color Frame Buffer
	// Retrieved Depth Frame Size
	IFrameDescription* pDepthDescription;
	hResult = DepthSource->get_FrameDescription(&pDepthDescription);
	if (FAILED(hResult))
	{
		LogKinectError("Depth Source Frame Description", hResult);
		return;
	}

	pDepthDescription->get_Width(&DepthWidth); // 512
	pDepthDescription->get_Height(&DepthHeight); // 424
	pDepthDescription->Release();
	ColorBuffer.Reset();
	DepthBuffer.Reset();
	SmoothDepthBuffer.Reset();
	ColorBuffer.AddUninitialized(ColorWidth * ColorHeight);
	DepthBuffer.AddUninitialized(DepthWidth * DepthHeight);
	SmoothDepthBuffer.AddUninitialized(DepthWidth * DepthHeight);
	if (bEnableBodyIndexMask)
	{
		BodyIndexBuffer.Reset();
		BodyIndexBuffer.AddUninitialized(DepthWidth * DepthHeight);
		hResult = Sensor->get_BodyIndexFrameSource(&BodyIndexSource);
		if (FAILED(hResult)){
			LogKinectError("Error : IKinectSensor::get_BodyIndexFrameSource()", hResult);
			return;
		}

		hResult = BodyIndexSource->OpenReader(&BodyIndexReader);
		if (FAILED(hResult)){
			LogKinectError("Error : IBodyIndexFrameSource::OpenReader()", hResult);
			return;
		}
	}
	else
	{
		BodyIndexBuffer.Empty();
	}
	if (Thread != nullptr)
	{
		delete Thread;
	}
	ConsumerFrame = 0;
	ProducerFrame = 0;
	Bodies.SetNum(6);
	UpdateBodies.SetNum(6);
	for (int i = 0; i < UpdateBodies.Num(); i++)
	{
		UpdateBodies[i].Joints.SetNum(JointType::JointType_Count);
	}
	bPlaying = true;
	if (Camera) Camera->UpdateResource();
	Thread = FRunnableThread::Create(this, TEXT("MeshGenerator"));
	Super::BeginPlay();
}


void AKinectActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	CommandDelegate Command;

	while (Commands.Dequeue(Command))
	{
		Command.Execute();
	}

}

void AKinectActor::UpdateBody()
{
	Bodies = UpdateBodies;
}

void AKinectActor::UpdateCamera()
{
	if (!CurrentCameraFrame.IsValid() || CurrentCameraFrame->Num() != ColorBuffer.Num() * 4)
	{
		TArray<uint8> *Copy = new TArray<uint8>();
		Copy->SetNumUninitialized(ColorBuffer.Num() * 4);
		CurrentCameraFrame = TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe>(Copy);
	}
	CurrentCameraFrame->Reset(ColorBuffer.Num() * 4);
	for (int i = 0; i < ColorBuffer.Num(); i++) {
		const RGBQUAD &q = ColorBuffer[i];
		CurrentCameraFrame->Add(q.rgbBlue);
		CurrentCameraFrame->Add(q.rgbGreen);
		CurrentCameraFrame->Add(q.rgbRed);
		CurrentCameraFrame->Add(1);
	}
	Camera->SetCurrentFrame(CurrentCameraFrame);
}

void AKinectActor::UpdateMesh()
{

	MeshComp->ClearAllMeshSections();
	MeshComp->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, EnablePhysics);
	ConsumerFrame = ProducerFrame;
}

void AKinectActor::DoUpdates(bool DoMesh)
{
	Crit.Lock();
	UpdateCamera();
	UpdateBody();
	if (DoMesh)
	{
		UpdateMesh();
	}
	Crit.Unlock();
}

uint32 AKinectActor::Run()
{
	while (bPlaying)
	{
		bool DoMeshData;
		Update();
		Crit.Lock();
		DoMeshData = ConsumerFrame >= ProducerFrame;
		Crit.Unlock();
		if (DoMeshData)
		{
			UpdateVertexData();
			Crit.Lock();
			ProducerFrame = ConsumerFrame + 1;
			Crit.Unlock();
		}
		Commands.Enqueue(FSimpleDelegate::CreateUObject(this, &AKinectActor::DoUpdates, DoMeshData));
		FPlatformProcess::Sleep(1.0f / 60);
	}
	return 0;
}


void AKinectActor::UpdateVertexData()
{
	if (bEnableDepthSmoothing)
	{
		FillHoles();
		//BilateralFilter();
	}
	TArray<UINT16> &DepthBuffer = bEnableDepthSmoothing ? this->SmoothDepthBuffer : this->DepthBuffer;
	Triangles.Reset();
	Vertices.Reset();
	VertexColors.Reset();
	CurrentFrame++;
	const int step = Resolution;
	const int startx = FMath::RoundToInt((DepthWidth - FMath::Clamp(ViewportWidth, 0.0f, 1.0f)*DepthWidth) / 2.0f);
	const int starty = FMath::RoundToInt((DepthHeight - FMath::Clamp(ViewportHeight, 0.0f, 1.0f)*DepthHeight) / 2.0f);

	for (int y = starty; y < DepthHeight - step - starty; y += step)
	{
		for (int x = startx; x < DepthWidth - step - startx; x += step)
		{
			FVector P[2][2];
			FColor C[2][2];
			const float scale = 100.0f; // meters to centimeters
			bool skip = false;
			for (int32 ix = 0; ix < 2 && !skip; ix++)
			{
				for (int32 iy = 0; iy < 2 && !skip; iy++)
				{
					int32 X = (x + ix*step);
					int32 Y = (y + iy*step);				
					UINT16 depth = DepthBuffer[Y * DepthWidth + X];
					if (BodyIndexBuffer.Num() > 0)
					{
						uint8 index = BodyIndexBuffer[Y * DepthWidth + X];
						skip = index == 255;
						if (!skip)
						{
							if (index >= 0 && index < BodyIndexMask.Num())
							{
								skip = !BodyIndexMask[index];
							}
							else
							{
								skip = true;
							}
						}
						if (skip) break;
					}
					DepthSpacePoint depthSpacePoint = { static_cast<float>(X), static_cast<float>(Y) };
					// Coordinate Mapping Depth to Color Space, and Setting PointCloud RGB
					ColorSpacePoint colorSpacePoint = { 0.0f, 0.0f };
					CoordinateMapper->MapDepthPointToColorSpace(depthSpacePoint, depth, &colorSpacePoint);
					int colorX = static_cast<int>(std::floor(colorSpacePoint.X + 0.5f));
					int colorY = static_cast<int>(std::floor(colorSpacePoint.Y + 0.5f));
					// Coordinate Mapping Depth to Camera Space, and Setting PointCloud XYZ
					CameraSpacePoint cameraSpacePoint = { 0.0f, 0.0f, 0.0f };
					CoordinateMapper->MapDepthPointToCameraSpace(depthSpacePoint, depth, &cameraSpacePoint);
					float dist = FMath::Sqrt(cameraSpacePoint.X * cameraSpacePoint.X +
						cameraSpacePoint.Y * cameraSpacePoint.Y +
						cameraSpacePoint.Z * cameraSpacePoint.Z);
					if (dist > MinDistanceInMeters && dist <= MaxDistanceInMeters &&
						(0 <= colorX) && (colorX < ColorWidth) && (0 <= colorY) && (colorY < ColorHeight))
					{
						RGBQUAD color = ColorBuffer[colorY * ColorWidth + colorX];
						C[ix][iy] = FColor(color.rgbRed, color.rgbGreen, color.rgbBlue);
						float right = cameraSpacePoint.X;
						float up = cameraSpacePoint.Y;
						float forward = cameraSpacePoint.Z;
						FVector P0;
						P0.X = forward * scale;
						P0.Y = right * scale;
						P0.Z = up * scale;
						P[ix][iy] = P0;
					}
					else
					{
						skip = true;
					}

				}
			}
			if (skip) continue;
			const FVector P00 = P[0][0];
			const FVector P01 = P[0][1];
			const FVector P10 = P[1][0];
			const FVector P11 = P[1][1];
			const int32 max_edge_len = MaxEdgeLength;
			if (((P00.X > 0) && (P01.X > 0) && (P10.X > 0) && (P11.X > 0) && (P01.X > 0) && (P10.X > 0) &&// check for non valid values
				(abs(P00.X - P01.X) < max_edge_len) &&
				(abs(P10.X - P01.X) < max_edge_len) &&
				(abs(P11.X - P01.X) < max_edge_len) &&
				(abs(P10.X - P01.X) < max_edge_len)))
			{
				const int32 Next = Vertices.Num();
				Vertices.Add(P00);
				Vertices.Add(P01);
				Vertices.Add(P10);

				Triangles.Add(Next);
				Triangles.Add(Next + 1);
				Triangles.Add(Next + 2);


				VertexColors.Add(C[0][0]);
				VertexColors.Add(C[0][1]);
				VertexColors.Add(C[1][0]);

				//Vertices.Add(P01);
				Triangles.Add(Next + 1);
				Vertices.Add(P11);
				Triangles.Add(Next + 3);
				//Vertices.Add(P10);
				Triangles.Add(Next + 2);
				//VertexColors.Add(C[0][1]);
				VertexColors.Add(C[1][1]);
				//VertexColors.Add(C[1][0]);
			}
		}

	}
}

static EJointType mapJointType(JointType type)
{
	switch (type)
	{
	case JointType::JointType_AnkleLeft:
		return EJointType::AnkleLeft;
	case JointType::JointType_AnkleRight:
		return EJointType::AnkleRight;
	case JointType::JointType_ElbowLeft:
		return EJointType::ElbowLeft;
	case JointType::JointType_ElbowRight:
		return EJointType::ElbowRight;
	case JointType::JointType_FootLeft:
		return EJointType::FootLeft;
	case JointType::JointType_FootRight:
		return EJointType::FootRight;
	case JointType::JointType_HandLeft:
		return EJointType::HandLeft;
	case JointType::JointType_HandRight:
		return EJointType::HandRight;
	case JointType::JointType_HandTipLeft:
		return EJointType::HandTipLeft;
	case JointType::JointType_HandTipRight:
		return EJointType::HandTipRight;
	case JointType::JointType_Head:
		return EJointType::Head;
	case JointType::JointType_HipLeft:
		return EJointType::HipLeft;
	case JointType::JointType_HipRight:
		return EJointType::HipRight;
	case JointType::JointType_KneeLeft:
		return EJointType::KneeLeft;
	case JointType::JointType_KneeRight:
		return EJointType::KneeRight;
	case JointType::JointType_Neck:
		return EJointType::Neck;
	case JointType::JointType_ShoulderLeft:
		return EJointType::ShoulderLeft;
	case JointType::JointType_ShoulderRight:
		return EJointType::ShoulderRight;
	case JointType::JointType_SpineBase:
		return EJointType::SpineBase;
	case JointType::JointType_SpineMid:
		return EJointType::SpineMid;
	case JointType::JointType_SpineShoulder:
		return EJointType::SpineShoulder;
	case JointType::JointType_ThumbLeft:
		return EJointType::ThumbLeft;
	case JointType::JointType_ThumbRight:
		return EJointType::ThumbRight;
	case JointType::JointType_WristLeft:
		return EJointType::WristLeft;
	case JointType::JointType_WristRight:
	default:
		return EJointType::WristRight;
	}

}

static ETrackingState mapTrackingState(TrackingState State)
{
	switch (State)
	{
	case TrackingState_NotTracked:
		return ETrackingState::NotTracked;
	case TrackingState_Inferred:
		return ETrackingState::Inferred;
	case TrackingState_Tracked:
	default:
		return ETrackingState::Tracked;
	}
}

void AKinectActor::DoUpdateBody()
{
	IBodyFrame *pBodyFrame;
	HRESULT hResult = BodyReader->AcquireLatestFrame(&pBodyFrame);
	if (SUCCEEDED(hResult))
	{
		IBody* pBody[BODY_COUNT] = { 0 };
		hResult = pBodyFrame->GetAndRefreshBodyData(BODY_COUNT, pBody);
		if (SUCCEEDED(hResult))
		{
			for (int count = 0; count < BODY_COUNT; count++)
			{
				BOOLEAN bTracked = false;
				hResult = pBody[count]->get_IsTracked(&bTracked);
				FBody &Body = UpdateBodies[count];
				Body.bIsTracked = !!bTracked;
				if (SUCCEEDED(hResult) && bTracked)
				{
					Joint joint[JointType::JointType_Count];
					JointOrientation jointOrient[JointType::JointType_Count];
					hResult = pBody[count]->GetJoints(JointType::JointType_Count, joint);
					if (SUCCEEDED(hResult))
					{
						hResult = pBody[count]->GetJointOrientations(JointType::JointType_Count, jointOrient);
						if (FAILED(hResult))
						{
							LogKinectError("GetJointOrientations", hResult);
						}
						for (int j = 0; j < JointType::JointType_Count; j++)
						{
							FJoint &J = Body.Joints[j];
							auto P = joint[j].Position;
							auto Forward = P.Z;
							auto Up = P.Y;
							auto Right = P.X;
							const float scale = 100.0f;
							J.Position.X = scale * Forward;
							J.Position.Y = scale * Right;
							J.Position.Z = scale * Up;
							J.JointType = mapJointType(joint[j].JointType);
							J.TrackingState = mapTrackingState(joint[j].TrackingState);
							for (int i = 0; i < JointType::JointType_Count; i++)
							{
								if (joint[j].JointType == jointOrient[i].JointType)
								{
									Vector4 &V = jointOrient[i].Orientation;
									J.Orientation = FRotator(FQuat(V.z, V.y, V.x, V.w));
									break;
								}
							}
							
						}
						// Left Hand State
						HandState leftHandState = HandState::HandState_Unknown;
						Body.LeftHandState = EHandState::Unknown;
						hResult = pBody[count]->get_HandLeftState(&leftHandState);
						if (SUCCEEDED(hResult))
						{
							switch (leftHandState)
							{
							case HandState::HandState_Open:
								Body.LeftHandState = EHandState::Open;
								break;
							case HandState::HandState_Closed:
								Body.LeftHandState = EHandState::Closed;
								break;
							case HandState::HandState_Lasso:
								Body.LeftHandState = EHandState::Lasso;
								break;
							}
						}

						// Right Hand State
						HandState rightHandState = HandState::HandState_Unknown;
						Body.RightHandState = EHandState::Unknown;
						hResult = pBody[count]->get_HandRightState(&rightHandState);
						if (SUCCEEDED(hResult))
						{
							switch (rightHandState)
							{
							case HandState::HandState_Open:
								Body.RightHandState = EHandState::Open;
								break;
							case HandState::HandState_Closed:
								Body.RightHandState = EHandState::Closed;
								break;
							case HandState::HandState_Lasso:
								Body.RightHandState = EHandState::Lasso;
								break;
							}
						}
					}

					// Activity
					UINT capacity = 0;
					DetectionResult detectionResults = DetectionResult::DetectionResult_Unknown;
					hResult = pBody[count]->GetActivityDetectionResults(capacity, &detectionResults);
					if (SUCCEEDED(hResult))
					{
						if (detectionResults == DetectionResult::DetectionResult_Yes)
						{
							switch (capacity)
							{
							case Activity::Activity_EyeLeftClosed:
								//std::cout << "Activity_EyeLeftClosed" << std::endl;
								break;
							case Activity::Activity_EyeRightClosed:
								//std::cout << "Activity_EyeRightClosed" << std::endl;
								break;
							case Activity::Activity_MouthOpen:
								//std::cout << "Activity_MouthOpen" << std::endl;
								break;
							case Activity::Activity_MouthMoved:
								//std::cout << "Activity_MouthMoved" << std::endl;
								break;
							case Activity::Activity_LookingAway:
								//std::cout << "Activity_LookingAway" << std::endl;
								break;
							default:
								break;
							}
						}
					}
					else
					{
						//std::cerr << "Error : IBody::GetActivityDetectionResults()" << std::endl;
					}

					// Appearance
					capacity = 0;
					detectionResults = DetectionResult::DetectionResult_Unknown;
					hResult = pBody[count]->GetAppearanceDetectionResults(capacity, &detectionResults);
					if (SUCCEEDED(hResult))
					{
						if (detectionResults == DetectionResult::DetectionResult_Yes)
						{
							switch (capacity)
							{
							case Appearance::Appearance_WearingGlasses:
								//std::cout << "Appearance_WearingGlasses" << std::endl;
								break;
							default:
								break;
							}
						}
					}
					else
					{
						//std::cerr << "Error : IBody::GetAppearanceDetectionResults()" << std::endl;
					}

					// Expression
					capacity = 0;
					detectionResults = DetectionResult::DetectionResult_Unknown;
					hResult = pBody[count]->GetExpressionDetectionResults(capacity, &detectionResults);
					if (SUCCEEDED(hResult))
					{
						if (detectionResults == DetectionResult::DetectionResult_Yes){
							switch (capacity)
							{
							case Expression::Expression_Happy:
								//std::cout << "Expression_Happy" << std::endl;
								break;
							case Expression::Expression_Neutral:
								//std::cout << "Expression_Neutral" << std::endl;
								break;
							default:
								break;
							}
						}
					}
					else
					{
						//std::cerr << "Error : IBody::GetExpressionDetectionResults()" << std::endl;
					}

					// Lean
					PointF amount;
					hResult = pBody[count]->get_Lean(&amount);
					if (SUCCEEDED(hResult))
					{
						//std::cout << "amount : " << amount.X << ", " << amount.Y << std::endl;
					}
				}
			}
		}
		for (int count = 0; count < BODY_COUNT; count++)
		{
			SafeRelease(pBody[count]);
		}
		SafeRelease(pBodyFrame);
	}
}

int AKinectActor::Update()
{
	if (ColorReader == nullptr) return 0;
	IColorFrame *pColorFrame = nullptr;
	HRESULT hResult = ColorReader->AcquireLatestFrame(&pColorFrame);
	if (SUCCEEDED(hResult)){
		// Retrieved Color Data
		TIMESPAN Timestamp;
		hResult = pColorFrame->get_RelativeTime(&Timestamp);
		if (SUCCEEDED(hResult))
		{
			if (Timestamp == CurrentFrame)
			{
				SafeRelease(pColorFrame);
				return 0;
			}

		}
		CurrentFrame = Timestamp;
		hResult = pColorFrame->CopyConvertedFrameDataToArray(ColorBuffer.Num() * sizeof(RGBQUAD),
			reinterpret_cast<BYTE*>(ColorBuffer.GetData()),
			ColorImageFormat::ColorImageFormat_Bgra);
		if (FAILED(hResult)){
			LogKinectError("Error : IColorFrame::CopyConvertedFrameDataToArray()", hResult);
		}
	}

	SafeRelease(pColorFrame);

	IDepthFrame *pDepthFrame = nullptr;
	// Acquire Latest Depth Frame
	hResult = DepthReader->AcquireLatestFrame(&pDepthFrame);
	if (SUCCEEDED(hResult)){
		// Retrieved Depth Data
		hResult = pDepthFrame->CopyFrameDataToArray(DepthBuffer.Num(), DepthBuffer.GetData());
		if (FAILED(hResult)){
			LogKinectError("Error : IDepthFrame::CopyFrameDataToArray()", hResult);
		}
	}

	SafeRelease(pDepthFrame);

	if (BodyIndexReader != nullptr)
	{
		IBodyIndexFrame *pBodyIndexFrame = nullptr;
		// Acquire Latest Body Index Frame
		hResult = BodyIndexReader->AcquireLatestFrame(&pBodyIndexFrame);
		if (SUCCEEDED(hResult)){
			// Retrieved Body Index Data
			hResult = pBodyIndexFrame->CopyFrameDataToArray(BodyIndexBuffer.Num(), BodyIndexBuffer.GetData());
		}
		SafeRelease(pBodyIndexFrame);
	}

	DoUpdateBody();
	return 0;
}

// Algorithm from http://stackoverflow.com/questions/5695865/bilateral-filter
void AKinectActor::BilateralFilter()
{
	const int width = DepthWidth;
	const int height = DepthHeight;
	UINT16 *_in = SmoothDepthBuffer.GetData();
	UINT16 *_out = SmoothDepthBuffer.GetData();
	const int halfkernelsize = FMath::RoundToInt(BilateralFilterKernelSize / 2.0f);
	const float id = 1.0f;
	const float cd = 1.0f;
	//void convolution(uchar4 *_in, uchar4 *_out, int width, int height, int ~halfkernelsize, float id, float cd)
	{
		int kernelDim = 2 * halfkernelsize + 1;

		Concurrency::parallel_for(0, height, [&](int y)
		{
			for (int x = 0; x < width; x++) {

				float sumWeight = 0;
				float _sum[3] = { 0, 0, 0 };
				unsigned int ctrIdx = y*width + x;

				float ctrPix[3];
				ctrPix[0] = _in[ctrIdx];
				ctrPix[1] = _in[ctrIdx];
				ctrPix[2] = _in[ctrIdx];


				// neighborhood of current pixel
				int kernelStartX, kernelEndX, kernelStartY, kernelEndY;
				kernelStartX = x - halfkernelsize;
				kernelEndX = x + halfkernelsize;
				kernelStartY = y - halfkernelsize;
				kernelEndY = y + halfkernelsize;

				for (int j = kernelStartY; j <= kernelEndY; j++)
				{
					for (int i = kernelStartX; i <= kernelEndX; i++)
					{
						unsigned int idx = FMath::Max(0, FMath::Min(j, height - 1))*width + FMath::Max(0, FMath::Min(i, width - 1));

						float curPix[3];
						curPix[0] = _in[idx];
						curPix[1] = _in[idx];
						curPix[2] = _in[idx];


						float currWeight;

						// define bilateral filter kernel weights
						float imageDist = sqrt((float)((i - x)*(i - x) + (j - y)*(j - y)));

						float colorDist = sqrt((float)((curPix[0] - ctrPix[0])*(curPix[0] - ctrPix[0]) +
							(curPix[1] - ctrPix[1])*(curPix[1] - ctrPix[1]) +
							(curPix[2] - ctrPix[2])*(curPix[2] - ctrPix[2])));

						currWeight = 1.0f / (exp((imageDist / id)*(imageDist / id)*0.5)*exp((colorDist / cd)*(colorDist / cd)*0.5));
						sumWeight += currWeight;

						_sum[0] += currWeight*curPix[0];
						_sum[1] += currWeight*curPix[1];
						_sum[2] += currWeight*curPix[2];
					}
				}

				_sum[0] /= sumWeight;
				_sum[1] /= sumWeight;
				_sum[2] /= sumWeight;

				_out[ctrIdx] = (UINT16)(floor(_sum[0]));
			}
		});
	}
}

// Algorithm from http://www.codeproject.com/Articles/317974/KinectDepthSmoothing
void AKinectActor::SmoothDepthImage()
{
	if (true)
	{
		BilateralFilter();
		return;
	}
	TArray<UINT16> &smoothDepthArray = SmoothDepthBuffer;
	TArray<UINT16> &depthArray = DepthBuffer;

	// We will be using these numbers for constraints on indexes
	int widthBound = DepthWidth - 1;
	int heightBound = DepthHeight - 1;
	Concurrency::parallel_for(0, DepthHeight, [&](int depthArrayRowIndex)
		// We process each row in parallel
	{
		// Process each pixel in the row
		for (int depthArrayColumnIndex = 0; depthArrayColumnIndex < DepthWidth; depthArrayColumnIndex++)
		{
			int depthIndex = depthArrayColumnIndex + (depthArrayRowIndex * DepthWidth);

			// We are only concerned with eliminating 'white' noise from the data.
			// We consider any pixel with a depth of 0 as a possible candidate for filtering.
			if (depthArray[depthIndex] == 0)
			{
				// From the depth index, we can determine the X and Y coordinates that the index
				// will appear in the image. We use this to help us define our filter matrix.
				int x = depthIndex % DepthWidth;
				int y = (depthIndex - x) / DepthWidth;

				// The filter collection is used to count the frequency of each
				// depth value in the filter array. This is used later to determine
				// the statistical mode for possible assignment to the candidate.
				UINT16 filterCollection[24][2];

				// The inner and outer band counts are used later to compare against the threshold 
				// values set in the UI to identify a positive filter result.
				int innerBandCount = 0;
				int outerBandCount = 0;

				// The following loops will loop through a 5 X 5 matrix of pixels surrounding the 
				// candidate pixel. This defines 2 distinct 'bands' around the candidate pixel.
				// If any of the pixels in this matrix are non-0, we will accumulate them and count
				// how many non-0 pixels are in each band. If the number of non-0 pixels breaks the
				// threshold in either band, then the average of all non-0 pixels in the matrix is applied
				// to the candidate pixel.
				for (int yi = -2; yi < 3; yi++)
				{
					for (int xi = -2; xi < 3; xi++)
					{
						// yi and xi are modifiers that will be subtracted from and added to the
						// candidate pixel's x and y coordinates that we calculated earlier. From the
						// resulting coordinates, we can calculate the index to be addressed for processing.

						// We do not want to consider the candidate
						// pixel (xi = 0, yi = 0) in our process at this point.
						// We already know that it's 0
						if (xi != 0 || yi != 0)
						{
							// We then create our modified coordinates for each pass
							int xSearch = x + xi;
							int ySearch = y + yi;

							// While the modified coordinates may in fact calculate out to an actual index, it 
							// might not be the one we want. Be sure to check
							// to make sure that the modified coordinates
							// match up with our image bounds.
							if (xSearch >= 0 && xSearch <= widthBound &&
								ySearch >= 0 && ySearch <= heightBound)
							{
								int index = xSearch + int(ySearch * DepthWidth);
								// We only want to look for non-0 values
								if (depthArray[index] != 0)
								{
									// We want to find count the frequency of each depth
									for (int i = 0; i < 24; i++)
									{
										if (filterCollection[i][0] == depthArray[index])
										{
											// When the depth is already in the filter collection
											// we will just increment the frequency.
											filterCollection[i][1]++;
											break;
										}
										else if (filterCollection[i][0] == 0)
										{
											// When we encounter a 0 depth in the filter collection
											// this means we have reached the end of values already counted.
											// We will then add the new depth and start it's frequency at 1.
											filterCollection[i][0] = depthArray[index];
											filterCollection[i][1]++;
											break;
										}
									}

									// We will then determine which band the non-0 pixel
									// was found in, and increment the band counters.
									if (yi != 2 && yi != -2 && xi != 2 && xi != -2)
										innerBandCount++;
									else
										outerBandCount++;
								}
							}
						}
					}
				}

				// Once we have determined our inner and outer band non-zero counts, and 
				// accumulated all of those values, we can compare it against the threshold
				// to determine if our candidate pixel will be changed to the
				// statistical mode of the non-zero surrounding pixels.
				if (innerBandCount >= InnerBandThreshold || outerBandCount >= OuterBandThreshold)
				{
					short frequency = 0;
					short depth = 0;
					// This loop will determine the statistical mode
					// of the surrounding pixels for assignment to
					// the candidate.
					for (int i = 0; i < 24; i++)
					{
						// This means we have reached the end of our
						// frequency distribution and can break out of the
						// loop to save time.
						if (filterCollection[i][0] == 0)
							break;
						if (filterCollection[i][1] > frequency)
						{
							depth = filterCollection[i][0];
							frequency = filterCollection[i][1];
						}
					}
					smoothDepthArray[depthIndex] = depth;
				}
				else {
					smoothDepthArray[depthIndex] = depthArray[depthIndex];
				}
			}
			else
			{
				// If the pixel is not zero, we will keep the original depth.
				smoothDepthArray[depthIndex] = depthArray[depthIndex];
			}
		}
	});
}


void AKinectActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bPlaying = false;
	Thread->WaitForCompletion();
	delete Thread;
	Thread = nullptr;
	SafeRelease(ColorSource);
	SafeRelease(DepthSource);
	SafeRelease(BodySource);
	SafeRelease(BodyIndexSource);
	SafeRelease(ColorReader);
	SafeRelease(DepthReader);
	SafeRelease(BodyReader);
	SafeRelease(BodyIndexReader);
	SafeRelease(CoordinateMapper);
	if (Sensor){
		Sensor->Close();
	}
	SafeRelease(Sensor);
}


static void findNeighbors(
	const UINT16 *Source,
	const int w,
	const int h,
	const int i,
	TArray<UINT16> &Result,
	const int radius,
	UINT16 &min,
	UINT16 &max,
	int &enclosed)
{
	const int y0 = i / h;
	const int x0 = i % h;
	min = 65535;
	max = 0;
	enclosed = 0;
	Result.Reset();
	for (int y = FMath::Max(0, y0 - radius); y < FMath::Min(y0 + radius, h); y++)
	{
		for (int x = FMath::Max(x0 - radius, 0); x < FMath::Min(x0 + radius, w); x++)
		{
			int j = y * w + x;
			if (j != i && j >= 0 && j < w * h)
			{
				UINT16 d = Source[j];
				if (d > 0)
				{
					min = FMath::Min(min, d);
					max = FMath::Max(max, d);

					bool onEdge = (y == y0 - radius) || (y + 1 == y0 + radius) || (x == x0 - radius) || (x + 1 == x0 + radius);
					if (onEdge)
					{
						enclosed++;
					}
					Result.Add(d);
				}
			}
		}
	}
	if (max < min) max = min;
}

// Algorithmn 1 from https://www.cs.unc.edu/~maimone/media/CG_paper_2012.pdf
void AKinectActor::FillHoles()
{
	UINT16 *depth_in = DepthBuffer.GetData();
	UINT16 *depth_out = SmoothDepthBuffer.GetData();
	const int radiusPass[2] = { HoleFillingRadius, SmoothingRadius };
	const int tc = Tc;
	const int te = Te;
	const int tr = Tr;
	for (int pass = 1; pass <= 2; pass++)
	{
		const int radius = radiusPass[pass - 1];
		Concurrency::combinable<TArray<UINT16>> combinedNeighbors;
		Concurrency::parallel_for(0, DepthBuffer.Num(), [&](int i)
		{
			depth_out[i] = depth_in[i];
			if (depth_in[i] == 0 || pass == 2)
			{
				int count = 0;
				int enclosed = 0;
				UINT16 min = 0;
				UINT16 max = 0;
				/*
				findNeighbors(
				const TArray<UINT16> &Source,
				const int w,
				const int h,
				const int i,
				TArray<UINT16> &Result,
				const int radius,
				UINT16 &min,
				UINT16 &max,
				int &enclosed
				)*/
				TArray<UINT16> &neighbors = combinedNeighbors.local(); // non-zero neighbors
				findNeighbors(depth_in, DepthWidth, DepthHeight, i, neighbors, radius, min, max, enclosed);
				if (max - min < tr && neighbors.Num() >= tc && enclosed >= te)
				{
					neighbors.Sort();
					depth_out[i] = neighbors[neighbors.Num() / 2];
				}
				else if (pass == 2)
				{
					//depth_out[i] = 0; // trim
				}
			}
		});
		if (pass == 1)
		{
			if (true) break;
			depth_in = depth_out;
		}
	}
}
