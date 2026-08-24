using System;
using NUnit.Framework;
using UnityEngine;

namespace XRBridge.Tests
{
    public sealed class XRBridgeEditModeTests
    {
        [Test]
        public void OpenXrPositionReflectsZAxis()
        {
            Assert.That(CoordinateConvention.OpenXrToUnity(new Vector3(1f, 2f, -3f)),
                Is.EqualTo(new Vector3(1f, 2f, 3f)));
        }

        [Test]
        public void OpenXrRotationMatchesReflectedForwardVector()
        {
            Quaternion openXr = Quaternion.AngleAxis(90f, Vector3.up);
            Quaternion unity = CoordinateConvention.OpenXrToUnity(openXr);
            Vector3 sourceRotated = openXr * Vector3.back;
            Vector3 expected = new Vector3(sourceRotated.x, sourceRotated.y, -sourceRotated.z);
            Assert.That(Vector3.Distance(unity * Vector3.forward, expected), Is.LessThan(1e-5f));
        }

        [Test]
        public void WrapperInitializesQueriesAndShutsDownBackend()
        {
            var backend = new FakeNativeBridge();
            var client = new XRBridgeClient(backend);
            Assert.That(backend.Initialized, Is.True);
            Assert.That(client.TryGetUnityPose(XRBridgeDevice.Head, 42, out NativePose pose,
                out XRBridgeResult result), Is.True);
            Assert.That(result, Is.EqualTo(XRBridgeResult.Ok));
            Assert.That(pose.TimestampNs, Is.EqualTo(42));
            client.Dispose();
            Assert.That(backend.ShutdownCalled, Is.True);
            Assert.Throws<ObjectDisposedException>(() => client.GetStats());
        }

        [Test]
        public void WrapperSurfacesNativeErrors()
        {
            var backend = new FakeNativeBridge { SubmitResult = XRBridgeResult.Duplicate };
            using var client = new XRBridgeClient(backend);
            XRBridgeException error = Assert.Throws<XRBridgeException>(() =>
                client.SubmitOpenXrPose(XRBridgeDevice.Head, new NativePose()));
            Assert.That(error.Result, Is.EqualTo(XRBridgeResult.Duplicate));
        }

        private sealed class FakeNativeBridge : INativeBridge
        {
            public bool Initialized { get; private set; }
            public bool ShutdownCalled { get; private set; }
            public XRBridgeResult SubmitResult { get; set; } = XRBridgeResult.Ok;

            public XRBridgeResult Initialize(ref NativeConfig config)
            {
                Initialized = true;
                return XRBridgeResult.Ok;
            }

            public XRBridgeResult Shutdown()
            {
                ShutdownCalled = true;
                return XRBridgeResult.Ok;
            }

            public XRBridgeResult Submit(XRBridgeDevice device, ref NativePose pose) => SubmitResult;

            public XRBridgeResult Query(XRBridgeDevice device, long timestampNs, out NativePose pose)
            {
                pose = new NativePose { TimestampNs = timestampNs };
                return XRBridgeResult.Ok;
            }

            public XRBridgeResult GetStats(out XRBridgeStats stats)
            {
                stats = new XRBridgeStats();
                return XRBridgeResult.Ok;
            }

            public string LastError() => "fake error";
        }
    }
}
