using UnityEngine;

namespace XRBridge
{
    public sealed class XRBridgeTrackedRig : MonoBehaviour
    {
        [SerializeField] private Transform head;
        [SerializeField] private Transform leftController;
        [SerializeField] private Transform rightController;

        private XRBridgeClient client;

        public string ConnectionStatus { get; private set; } = "Not started";
        public double LatestSampleAgeMs { get; private set; }
        public ulong DroppedSampleCount { get; private set; }

        public void Configure(Transform headTarget, Transform leftTarget, Transform rightTarget)
        {
            head = headTarget;
            leftController = leftTarget;
            rightController = rightTarget;
        }

        private void Awake()
        {
            try
            {
                client = new XRBridgeClient(256, 300_000_000);
                ConnectionStatus = "Native bridge connected";
            }
            catch (System.Exception error)
            {
                ConnectionStatus = error.Message;
                enabled = false;
            }
        }

        private void Update()
        {
            long timestamp = MonotonicClock.NowNanoseconds;
            UpdateTarget(XRBridgeDevice.Head, head, timestamp);
            UpdateTarget(XRBridgeDevice.LeftController, leftController, timestamp);
            UpdateTarget(XRBridgeDevice.RightController, rightController, timestamp);
            XRBridgeStats stats = client.GetStats();
            DroppedSampleCount = stats.DroppedOverflow;
        }

        private void UpdateTarget(XRBridgeDevice device, Transform target, long timestamp)
        {
            if (target == null)
            {
                return;
            }
            if (!client.TryGetUnityPose(device, timestamp, out NativePose nativePose,
                    out XRBridgeResult result))
            {
                if (result != XRBridgeResult.Unavailable && result != XRBridgeResult.Stale)
                {
                    ConnectionStatus = result.ToString();
                }
                return;
            }

            Pose pose = CoordinateConvention.ToUnityPose(nativePose);
            target.SetPositionAndRotation(pose.position, pose.rotation);
            LatestSampleAgeMs = (timestamp - nativePose.TimestampNs) / 1_000_000.0;
        }

        private void OnDestroy()
        {
            client?.Dispose();
            client = null;
        }
    }
}
