To use this plugin you'll need to install the Kinect for Windows SDK 2.0

http://www.microsoft.com/en-us/download/details.aspx?id=44561

The Plugin contains a Blueprint <code>KinectActor</code> which provides realtime mesh reconstruction, camera texture, and body tracking. The actor contains a <code>UProceduralMeshComponent</code> which will contain the mesh reconstruction. Note that the mesh uses per-vertex colors so you'll need to assign it a [Vertex Color  Material](https://docs.unrealengine.com/latest/INT/Engine/UI/LevelEditor/Modes/MeshPaintMode/VertexColor/MaterialSetup/Color/index.html).

The actor has the following Blueprint properties:

    TArray<Body> Bodies
  
    UTexture Camera
  
    bool bEnableBodyIndexMask
  
    TArray<bool> BodyIndexMask
    
If you enable the <code>Body Index Mask</code>, then any points in the depth camera image that do not correspond to one of the specified bodies will not be triangulated in the mesh.
    
Each <code>Body</code> struct has the following properties:

    bool bIsTracked
    
    TArray<Joint> Joints
    
Each <code>Joint</code> struct has the following properties:

    ETrackingStatus TrackingStatus
    
    FVector Position
    
    FRotator Orientation
    
    EJointType JointType 

The plugin also contains a Blueprint <code>KinectFusionActor</code> which contains a UProceduralMeshComponent that provides the output of the kinect
fusion mesh reconstruction.
