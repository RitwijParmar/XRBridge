using System;

namespace XRBridge
{
    public sealed class XRBridgeException : Exception
    {
        public XRBridgeException(XRBridgeResult result, string message)
            : base($"XRBridge {result}: {message}")
        {
            Result = result;
        }

        public XRBridgeResult Result { get; }
    }

    public sealed class XRBridgeClient : IDisposable
    {
        private static readonly object SharedLock = new object();
        private static INativeBridge sharedNative;
        private static int sharedUsers;

        private readonly INativeBridge native;
        private readonly bool shared;
        private bool disposed;

        public XRBridgeClient(uint capacityPerDevice = 256, long maxSampleAgeNs = 100_000_000)
        {
            if (capacityPerDevice == 0 || maxSampleAgeNs < 0)
            {
                throw new ArgumentOutOfRangeException(nameof(capacityPerDevice));
            }
            lock (SharedLock)
            {
                if (sharedUsers == 0)
                {
                    sharedNative = new PInvokeNativeBridge();
                    var config = new NativeConfig
                    {
                        CapacityPerDevice = capacityPerDevice,
                        MaxSampleAgeNs = maxSampleAgeNs,
                    };
                    XRBridgeResult result = sharedNative.Initialize(ref config);
                    if (result != XRBridgeResult.Ok)
                    {
                        throw new XRBridgeException(result, sharedNative.LastError());
                    }
                }
                native = sharedNative;
                sharedUsers++;
                shared = true;
            }
        }

        internal XRBridgeClient(INativeBridge native, uint capacityPerDevice = 256,
            long maxSampleAgeNs = 100_000_000)
        {
            this.native = native ?? throw new ArgumentNullException(nameof(native));
            if (capacityPerDevice == 0 || maxSampleAgeNs < 0)
            {
                throw new ArgumentOutOfRangeException(nameof(capacityPerDevice));
            }

            var config = new NativeConfig
            {
                CapacityPerDevice = capacityPerDevice,
                MaxSampleAgeNs = maxSampleAgeNs,
            };
            ThrowIfFailed(native.Initialize(ref config));
        }

        public void SubmitOpenXrPose(XRBridgeDevice device, NativePose pose)
        {
            ThrowIfDisposed();
            ThrowIfFailed(native.Submit(device, ref pose));
        }

        public bool TryGetUnityPose(XRBridgeDevice device, long timestampNs, out NativePose pose,
            out XRBridgeResult result)
        {
            ThrowIfDisposed();
            result = native.Query(device, timestampNs, out pose);
            return result == XRBridgeResult.Ok;
        }

        public XRBridgeStats GetStats()
        {
            ThrowIfDisposed();
            ThrowIfFailed(native.GetStats(out XRBridgeStats stats));
            return stats;
        }

        public void Dispose()
        {
            if (disposed)
            {
                return;
            }

            XRBridgeResult result = XRBridgeResult.Ok;
            if (shared)
            {
                lock (SharedLock)
                {
                    sharedUsers--;
                    if (sharedUsers == 0)
                    {
                        result = native.Shutdown();
                        sharedNative = null;
                    }
                }
            }
            else
            {
                result = native.Shutdown();
            }
            disposed = true;
            if (result != XRBridgeResult.Ok && result != XRBridgeResult.NotInitialized)
            {
                throw new XRBridgeException(result, native.LastError());
            }
        }

        private void ThrowIfFailed(XRBridgeResult result)
        {
            if (result != XRBridgeResult.Ok)
            {
                throw new XRBridgeException(result, native.LastError());
            }
        }

        private void ThrowIfDisposed()
        {
            if (disposed)
            {
                throw new ObjectDisposedException(nameof(XRBridgeClient));
            }
        }
    }
}
