#pragma once

#include "Engine.h"
#include "GameFramework/Actor.h"
#include "Engine/Texture.h"
#include "ProceduralMeshComponent.h"
#include "IKinectPlugin.h"
#include "KinectFusionActor.generated.h"

UCLASS()
class KINECTPLUGIN_API AKinectFusionActor : public AActor, public FRunnable
{
  GENERATED_UCLASS_BODY()
    public:
  UPROPERTY(Category="Kinect", EditAnywhere, BlueprintReadOnly)
    bool EnablePhysics;
  UPROPERTY(Category="Kinect", EditAnywhere, BlueprintReadOnly)
    UTexture *Camera;
  UPROPERTY(Category="Kinect", EditAnywhere, BlueprintReadOnly)
    UTexture *DepthCamera;
  UPROPERTY(Category="Kinect", EditAnywhere, BlueprintReadOnly)
    UTexture *InfraredCamera;
  UPROPERTY(Category="Kinect", EditAnywhere)
  UProceduralMeshComponent *MeshComp;

  virtual void BeginPlay() override;
 
  virtual void Tick(float DeltaSeconds) override;
  
  virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

  virtual ~AKinectFusionActor();
    private:
  int Update();
  int UpdateVertexData(struct INuiFusionColorMesh *mesh);

  class KinectFusionProcessor *Processor;
  TArray<int32> Triangles;
  TArray<FVector> Vertices;
  TArray<FVector> Normals;
  TArray<FVector2D> UVs;
  TArray<FColor> VertexColors;
  TArray<FProcMeshTangent> Tangents;
  float LastReconstruction;
  FRunnableThread *Thread;
  TQueue<struct INuiFusionColorMesh *, EQueueMode::Mpsc> MeshGeneration;
  int PlayCount;
public:
	uint32 Run() override;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
	int32 VoxelsPerMeter = 256 / 2;    // 1000mm / 256vpm = ~3.9mm/voxel
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
	int32 VoxelCountX;// = 384;       // 384 / 256vpm = 1.5m wide reconstruction
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
	int32 VoxelCountY;// = 384;       // Memory = 384*384*384 * 4bytes per voxel
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
	int32 VoxelCountZ;// = 384;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          bool AutoResetReconstructionWhenLost;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          bool AutoResetReconstructionOnTimeout; // We now try to find the camera pose, however, setting this false will no longer auto reset on .xef file playback
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          bool AutoFindCameraPoseWhenLost;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          int32 MinDepthThreshold;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          int32 MaxDepthThreshold; 
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          bool MirrorDepthFrame;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          int32 MaxIntegrationWeight;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          bool DisplaySurfaceNormals;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          bool CaptureColor;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          int32 ColorIntegrationInterval;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          bool TranslateResetPoseByMinDepthThreshold;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          int32 DeltaFromReferenceFrameCalculationInterval;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          int32 MinSuccessfulTrackingFramesForCameraPoseFinder; // only update the camera pose finder initially after 45 successful frames (1.5s)
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          int32 MinSuccessfulTrackingFramesForCameraPoseFinderAfterFailure; // resume integration following 200 successful frames after tracking failure (~7s)
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          int32 MaxCameraPoseFinderPoseHistory;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          int32 CameraPoseFinderFeatureSampleLocationsPerFrame;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          float MaxCameraPoseFinderDepthThreshold;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          float CameraPoseFinderDistanceThresholdReject; // a value of 1.0 means no rejection
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          float CameraPoseFinderDistanceThresholdAccept;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          int32 MaxCameraPoseFinderPoseTests;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          int32 CameraPoseFinderProcessFrameCalculationInterval;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          float MaxAlignToReconstructionEnergyForSuccess;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          float MinAlignToReconstructionEnergyForSuccess;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          float MaxAlignPointCloudsEnergyForSuccess;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          float MinAlignPointCloudsEnergyForSuccess;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          int32 SmoothingKernelWidth;                 // 0=just copy, 1=3x3, 2=5x5, 3=7x7, here we create a 3x3 kernel
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          float SmoothingDistanceThreshold;       // 4cm, could use up to around 0.1f
          int32 AlignPointCloudsImageDownsampleFactor; //2=x/2,y/2, 4=x/4,y/4
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          float MaxTranslationDelta;               // 0.15 - 0.3m per frame typical
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadWrite)
          float MaxRotationDelta;                  // 10-20 degrees per frame typical

};
