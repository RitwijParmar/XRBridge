using UnityEngine;

namespace XRBridge.Samples
{
    public sealed class XRBridgeDemoBootstrap : MonoBehaviour
    {
        private XRBridgeTrackedRig rig;

        private void Awake()
        {
            Camera camera = Camera.main;
            if (camera != null)
            {
                camera.transform.SetPositionAndRotation(new Vector3(0f, 1.8f, -4.5f),
                    Quaternion.identity);
            }

            Transform head = CreateTrackedObject("Headset", PrimitiveType.Cube,
                new Vector3(0.28f, 0.16f, 0.14f), new Color(0.2f, 0.8f, 1f));
            Transform left = CreateTrackedObject("Left Controller", PrimitiveType.Capsule,
                new Vector3(0.08f, 0.16f, 0.08f), new Color(1f, 0.45f, 0.2f));
            Transform right = CreateTrackedObject("Right Controller", PrimitiveType.Capsule,
                new Vector3(0.08f, 0.16f, 0.08f), new Color(0.4f, 1f, 0.35f));

            rig = gameObject.AddComponent<XRBridgeTrackedRig>();
            rig.Configure(head, left, right);
            TextAsset trace = Resources.Load<TextAsset>("xrbridge_demo_trace");
            XRBridgeReplay replay = gameObject.AddComponent<XRBridgeReplay>();
            replay.Configure(trace);
        }

        private static Transform CreateTrackedObject(string objectName, PrimitiveType primitive,
            Vector3 scale, Color color)
        {
            GameObject tracked = GameObject.CreatePrimitive(primitive);
            tracked.name = objectName;
            tracked.transform.localScale = scale;
            tracked.GetComponent<Renderer>().material.color = color;
            TrailRenderer trail = tracked.AddComponent<TrailRenderer>();
            trail.time = 2.5f;
            trail.startWidth = 0.025f;
            trail.endWidth = 0.002f;
            trail.material = new Material(Shader.Find("Sprites/Default"));
            trail.startColor = color;
            trail.endColor = new Color(color.r, color.g, color.b, 0f);
            return tracked.transform;
        }

        private void OnGUI()
        {
            if (rig == null)
            {
                return;
            }
            var style = new GUIStyle(GUI.skin.box)
            {
                alignment = TextAnchor.UpperLeft,
                fontSize = 18,
                normal = { textColor = Color.white },
            };
            string status = $"XRBridge\n{rig.ConnectionStatus}\n" +
                            $"sample age: {rig.LatestSampleAgeMs:F2} ms\n" +
                            $"dropped: {rig.DroppedSampleCount}";
            GUI.Box(new Rect(20f, 20f, 360f, 120f), status, style);
        }
    }
}
