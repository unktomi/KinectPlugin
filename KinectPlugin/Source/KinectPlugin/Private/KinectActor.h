#pragma once

#include "Engine.h"
#include "GameFramework/Actor.h"
#include "Engine/Texture.h"
#include "ProceduralMeshComponent.h"
#include "IKinectPlugin.h"
#include "KinectTexture.h"
#include "AllowWindowsPlatformTypes.h"
#include "Kinect.h"
#include "HideWindowsPlatformTypes.h"
#include "KinectActor.generated.h"

UENUM(BlueprintType)
enum class EHand : uint8
{
	LeftHand, RightHand
};

UENUM(BlueprintType)
enum class EHandState : uint8
{
	Closed, Lasso, NotTracked, Open, Unknown
};


UENUM(BlueprintType)
enum class ETrackingState : uint8
{
	Inferred, NotTracked, Tracked
};

UENUM(BlueprintType)
enum class EJointType: uint8
{
	AnkleLeft	/* 14 */ 	UMETA(DisplayName="Left ankle"),
	AnkleRight	/* 18 */ 	UMETA(DisplayName = "Right ankle"),
	ElbowLeft	/* 5 */ 	UMETA(DisplayName = "Left elbow"),
	ElbowRight	/* 9 */ 	UMETA(DisplayName = "Right elbow"),
	FootLeft	/* 15 */	UMETA(DisplayName = "Left foot"),
	FootRight	/* 19 */ 	UMETA(DisplayName = "Right foot"),
	HandLeft	/* 7 */ 	UMETA(DisplayName = "Left hand"),
	HandRight	/* 11 */ 	UMETA(DisplayName = "Right hand"),
	HandTipLeft	/* 21 */ 	UMETA(DisplayName = "Tip of the left hand"),
	HandTipRight	/* 23 */ 	UMETA(DisplayName = "Tip of the right hand"),
	Head	/* 3 */ 	UMETA(DisplayName = "Head"),
	HipLeft	/* 12 */ 	UMETA(DisplayName = "Left hip"),
	HipRight	/* 16 */ 	UMETA(DisplayName = "Right hip"),
	KneeLeft	/* 13 */ 	UMETA(DisplayName = "Left knee"),
	KneeRight	/* 17 */ 	UMETA(DisplayName = "Right knee"),
	Neck	/* 2 */ 	UMETA(DisplayName="Neck"),
	ShoulderLeft	/* 4 */ 	UMETA(DisplayName = "Left shoulder"),
	ShoulderRight	/* 8 */ 	UMETA(DisplayName = "Right shoulder"),
	SpineBase	/* 0 */ 	UMETA(DisplayName = "Base of the spine"),
	SpineMid	/* 1 */ 	UMETA(DisplayName = "Middle of the spine"),
	SpineShoulder	/* 20 */ 	UMETA(DisplayName = "Spine at the shoulder"),
	ThumbLeft	/* 22 */ 	UMETA(DisplayName = "Left thumb"),
	ThumbRight	/* 24 */ 	UMETA(DisplayName = "Right thumb"),
	WristLeft	/* 6 */ 	UMETA(DisplayName = "Left wrist"),
	WristRight	/* 10 */ 	UMETA(DisplayName = "Right wrist")
};

USTRUCT(BlueprintType)
struct FJoint
{
	GENERATED_USTRUCT_BODY();
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		EJointType JointType;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FVector Position;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FRotator Orientation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		ETrackingState TrackingState;
};

USTRUCT(BlueprintType)
struct FBody
{
	GENERATED_USTRUCT_BODY();
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
		TArray<FJoint> Joints;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
		bool bIsTracked;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
		EHandState LeftHandState;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
		EHandState RightHandState;
};


UCLASS()
class KINECTPLUGIN_API AKinectActor : public AActor, public FRunnable
{
	GENERATED_UCLASS_BODY()
public:

	UFUNCTION(Category = "Kinect", BlueprintCallable)
		EHandState GetHandState(int32 BodyIndex, EHand Hand)
	{
		FBody &Body = Bodies[BodyIndex];
		if (Hand == EHand::LeftHand)
		{
			return Body.LeftHandState;
		}
		return Body.RightHandState;
	}

	UFUNCTION(Category = "Kinect", BlueprintCallable)
		void GetJoint(int32 BodyIndex, EJointType JointType, FJoint &Result)
	{
		FBody &Body = Bodies[BodyIndex];
		for (int j = 0; j < Body.Joints.Num(); j++)
		{
			if (Body.Joints[j].JointType == JointType)
			{
				Result = Body.Joints[j];
			}
		}
	}

	UFUNCTION(Category = "Kinect", BlueprintCallable)
		bool IsBodyTracked(int32 BodyIndex)
	{
		return Bodies[BodyIndex].bIsTracked;
	}
	
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadOnly)
		bool EnablePhysics;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadOnly)
		UKinectTexture *Camera;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadOnly)
		TArray<FBody> Bodies;
	UPROPERTY()
		UTexture *DepthCamera;
	UPROPERTY()
		UTexture *InfraredCamera;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadOnly)
		bool bEnableBodyIndexMask;
	UPROPERTY(Category = "Kinect", EditAnywhere, BlueprintReadOnly)
		TArray<bool> BodyIndexMask;
	UPROPERTY(Category = "Kinect", EditAnywhere)
		UProceduralMeshComponent *MeshComp;
	UPROPERTY(Category = "Kinect", EditAnywhere)
		float ViewportWidth;
	UPROPERTY(Category = "Kinect", EditAnywhere)
		float ViewportHeight;
	UPROPERTY(Category = "Kinect", EditAnywhere)
		float MinDistanceInMeters;
	UPROPERTY(Category = "Kinect", EditAnywhere)
		float MaxDistanceInMeters;
	UPROPERTY(Category = "Kinect", EditAnywhere)
		int32 Resolution;
	UPROPERTY(Category = "Kinect", EditAnywhere)
		int32 MaxEdgeLength;
	UPROPERTY(Category = "Kinect", EditAnywhere)
		bool bEnableDepthSmoothing;
	UPROPERTY(Category = "Kinect", EditAnywhere)
		int32 BilateralFilterKernelSize;
	UPROPERTY(Category = "Kinect", EditAnywhere)
		int32 InnerBandThreshold;
	UPROPERTY(Category = "Kinect", EditAnywhere)
		int32 OuterBandThreshold;

	UPROPERTY(Category = "Kinect", EditAnywhere)
		int32 HoleFillingRadius;
	UPROPERTY(Category = "Kinect", EditAnywhere)
		int32 SmoothingRadius;

	UPROPERTY(Category = "Kinect", EditAnywhere)
		int32 Tc;
	UPROPERTY(Category = "Kinect", EditAnywhere)
		int32 Te;
	UPROPERTY(Category = "Kinect", EditAnywhere)
		int32 Tr;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual ~AKinectActor();
	virtual uint32 Run() override;

	/** Events have to occur on the main thread, so we have this queue to feed the ticker */
	DECLARE_DELEGATE(CommandDelegate)
	/** Holds the router command queue. */
	TQueue<CommandDelegate, EQueueMode::Mpsc> Commands;

	void UpdateMesh();
	void UpdateCamera();
	void UpdateBody();
	void DoUpdates(bool DoMesh);

	void UpdateVertexData();
	void DoUpdateBody();
private:
	void SmoothDepthImage();
	void BilateralFilter();
	void FillHoles();
	bool bPlaying;
	FRunnableThread *Thread;	
	FCriticalSection Crit;
	TIMESPAN ConsumerFrame;
	TIMESPAN ProducerFrame;
	TIMESPAN CurrentFrame;
	int Update();
	TArray<int32> Triangles;
	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	IKinectSensor *Sensor;
	ICoordinateMapper *CoordinateMapper;
	IColorFrameSource *ColorSource;
	IDepthFrameSource *DepthSource;
	IBodyFrameSource *BodySource;
	IBodyIndexFrameSource *BodyIndexSource;
	IColorFrameReader *ColorReader;
	IDepthFrameReader *DepthReader;
	IBodyFrameReader *BodyReader;
	IBodyIndexFrameReader *BodyIndexReader;
	int32 ColorWidth;
	int32 ColorHeight;
	int32 DepthWidth;
	int32 DepthHeight;
	TArray<UINT16> DepthBuffer;
	TArray<UINT16> SmoothDepthBuffer;
	TArray<RGBQUAD> ColorBuffer;
	TArray<uint8> BodyIndexBuffer;
	TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> CurrentCameraFrame;

	TArray<FBody> UpdateBodies; 

};


