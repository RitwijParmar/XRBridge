using System;
using UnityEngine;

namespace XRBridge
{
    public sealed class XRBridgeReplay : MonoBehaviour
    {
        [Serializable]
        private struct ReplaySample
        {
            public int device;
            public long timestampNs;
            public double px;
            public double py;
            public double pz;
            public double qx;
            public double qy;
            public double qz;
            public double qw;
        }

        [Serializable]
        private sealed class ReplayTrace
        {
            public ReplaySample[] samples;
        }

        [SerializeField] private TextAsset trace;
        [SerializeField] private bool loop = true;

        private XRBridgeClient client;
        private ReplayTrace replay;
        private int cursor;
        private long epochNs;
        private long durationNs;

        public void Configure(TextAsset traceAsset, bool shouldLoop = true)
        {
            trace = traceAsset;
            loop = shouldLoop;
        }

        private void Start()
        {
            if (trace == null)
            {
                enabled = false;
                return;
            }
            replay = JsonUtility.FromJson<ReplayTrace>(trace.text);
            if (replay?.samples == null || replay.samples.Length == 0)
            {
                enabled = false;
                return;
            }
            client = new XRBridgeClient();
            epochNs = MonotonicClock.NowNanoseconds;
            durationNs = replay.samples[replay.samples.Length - 1].timestampNs + 1;
        }

        private void Update()
        {
            long elapsed = MonotonicClock.NowNanoseconds - epochNs;
            while (cursor < replay.samples.Length && replay.samples[cursor].timestampNs <= elapsed)
            {
                ReplaySample sample = replay.samples[cursor++];
                var pose = new NativePose
                {
                    Position = new NativeVector3(sample.px, sample.py, sample.pz),
                    Rotation = new NativeQuaternion(sample.qx, sample.qy, sample.qz, sample.qw),
                    TimestampNs = epochNs + sample.timestampNs,
                };
                client.SubmitOpenXrPose((XRBridgeDevice)sample.device, pose);
            }
            if (loop && cursor == replay.samples.Length && elapsed >= durationNs)
            {
                cursor = 0;
                epochNs = MonotonicClock.NowNanoseconds;
            }
        }

        private void OnDestroy()
        {
            client?.Dispose();
            client = null;
        }
    }
}
