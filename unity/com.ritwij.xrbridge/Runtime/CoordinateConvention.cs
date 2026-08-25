using UnityEngine;

namespace XRBridge
{
    public static class CoordinateConvention
    {
        public static Vector3 OpenXrToUnity(Vector3 position) =>
            new Vector3(position.x, position.y, -position.z);

        public static Quaternion OpenXrToUnity(Quaternion rotation)
        {
            Quaternion converted = new Quaternion(-rotation.x, -rotation.y, rotation.z, rotation.w);
            return Quaternion.Normalize(converted);
        }

        public static Pose ToUnityPose(NativePose pose) => new Pose(
            new Vector3((float)pose.Position.X, (float)pose.Position.Y, (float)pose.Position.Z),
            new Quaternion((float)pose.Rotation.X, (float)pose.Rotation.Y,
                (float)pose.Rotation.Z, (float)pose.Rotation.W));
    }
}
