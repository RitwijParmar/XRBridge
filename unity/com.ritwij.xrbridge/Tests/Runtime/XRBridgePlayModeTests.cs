using System.Collections;
using NUnit.Framework;
using UnityEngine;
using UnityEngine.TestTools;

namespace XRBridge.Tests
{
    public sealed class XRBridgePlayModeTests
    {
        [UnityTest]
        public IEnumerator MonotonicClockAdvancesAcrossFrame()
        {
            long before = MonotonicClock.NowNanoseconds;
            yield return null;
            Assert.That(MonotonicClock.NowNanoseconds, Is.GreaterThan(before));
        }
    }
}
