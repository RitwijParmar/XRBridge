using System;
using System.Runtime.InteropServices;

namespace XRBridge
{
    public enum XRBridgeResult
    {
        Ok = 0,
        NotInitialized = 1,
        InvalidArgument = 2,
        InvalidQuaternion = 3,
        OutOfOrder = 4,
        Duplicate = 5,
        Unavailable = 6,
        Stale = 7,
        InternalError = 8,
    }

    public enum XRBridgeDevice
    {
        Head = 0,
        LeftController = 1,
        RightController = 2,
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct NativeVector3
    {
        public double X;
        public double Y;
        public double Z;

        public NativeVector3(double x, double y, double z)
        {
            X = x;
            Y = y;
            Z = z;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct NativeQuaternion
    {
        public double X;
        public double Y;
        public double Z;
        public double W;

        public NativeQuaternion(double x, double y, double z, double w)
        {
            X = x;
            Y = y;
            Z = z;
            W = w;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct NativePose
    {
        public NativeVector3 Position;
        public NativeQuaternion Rotation;
        public long TimestampNs;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct NativeConfig
    {
        public uint CapacityPerDevice;
        public long MaxSampleAgeNs;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct XRBridgeStats
    {
        public ulong Submitted;
        public ulong Queries;
        public ulong Interpolated;
        public ulong RejectedInvalid;
        public ulong RejectedOutOfOrder;
        public ulong RejectedDuplicate;
        public ulong DroppedOverflow;
        public ulong StaleQueries;
        public ulong UnavailableQueries;
    }

    internal interface INativeBridge
    {
        XRBridgeResult Initialize(ref NativeConfig config);
        XRBridgeResult Shutdown();
        XRBridgeResult Submit(XRBridgeDevice device, ref NativePose pose);
        XRBridgeResult Query(XRBridgeDevice device, long timestampNs, out NativePose pose);
        XRBridgeResult GetStats(out XRBridgeStats stats);
        string LastError();
    }

    internal sealed class PInvokeNativeBridge : INativeBridge
    {
        private const string LibraryName = "xrbridge";

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        private static extern XRBridgeResult xrbridge_initialize(ref NativeConfig config);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        private static extern XRBridgeResult xrbridge_shutdown();

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        private static extern XRBridgeResult xrbridge_submit_openxr_pose(
            XRBridgeDevice device,
            ref NativePose pose);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        private static extern XRBridgeResult xrbridge_query_unity_pose(
            XRBridgeDevice device,
            long timestampNs,
            out NativePose pose);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        private static extern XRBridgeResult xrbridge_get_stats(out XRBridgeStats stats);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr xrbridge_last_error();

        public XRBridgeResult Initialize(ref NativeConfig config) => xrbridge_initialize(ref config);
        public XRBridgeResult Shutdown() => xrbridge_shutdown();
        public XRBridgeResult Submit(XRBridgeDevice device, ref NativePose pose) =>
            xrbridge_submit_openxr_pose(device, ref pose);
        public XRBridgeResult Query(XRBridgeDevice device, long timestampNs, out NativePose pose) =>
            xrbridge_query_unity_pose(device, timestampNs, out pose);
        public XRBridgeResult GetStats(out XRBridgeStats stats) => xrbridge_get_stats(out stats);

        public string LastError()
        {
            IntPtr pointer = xrbridge_last_error();
            return pointer == IntPtr.Zero ? string.Empty : Marshal.PtrToStringAnsi(pointer) ?? string.Empty;
        }
    }
}
