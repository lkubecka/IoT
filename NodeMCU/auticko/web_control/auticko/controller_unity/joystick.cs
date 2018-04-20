using System.Collections;
using UnityEngine;
using UnityEngine.UI;
using UnityEngine.EventSystems;
using System.Threading;
using UnityEngine.Networking;

public class joystick : MonoBehaviour, IDragHandler, IPointerUpHandler, IPointerDownHandler {
    private RawImage bgImage;
    private RawImage joystickImg;
    public Vector3 InputDirection { set; get; }
    string url = "http://192.168.4.1/move";
    WWW www;
    private float timeMax = 250;
    private float timeSpent = 0;

    public virtual void OnDrag(PointerEventData ped) {
        Vector2 pos = Vector2.zero;
        if (RectTransformUtility.ScreenPointToLocalPointInRectangle(
            bgImage.rectTransform, 
            ped.position, 
            ped.pressEventCamera, 
            out pos)){

            pos.x = pos.x / bgImage.rectTransform.sizeDelta.x;
            pos.y = pos.y / bgImage.rectTransform.sizeDelta.y;

            float x = (bgImage.rectTransform.pivot.x == 1) ? pos.x * 2 + 1 : pos.x * 2 - 1;
            float y = (bgImage.rectTransform.pivot.y == 1) ? pos.y * 2 + 1 : pos.y * 2 - 1;

            InputDirection = new Vector3(x, 0, y);

            InputDirection = (InputDirection.magnitude > 1) ? InputDirection.normalized : InputDirection;

            joystickImg.rectTransform.anchoredPosition = new Vector3(InputDirection.x * (bgImage.rectTransform.sizeDelta.x / 3)
                , InputDirection.z * (bgImage.rectTransform.sizeDelta.y / 3));

        }
    }


    public virtual void OnPointerUp(PointerEventData ped)
    {
        OnDrag(ped);
    }

    public virtual void OnPointerDown(PointerEventData ped)
    {
        InputDirection = Vector3.zero;
        joystickImg.rectTransform.anchoredPosition = Vector3.zero;
    }

    void Start () {
        bgImage = GetComponent<RawImage>();
        joystickImg = transform.GetChild(0).GetComponent<RawImage>();
        InputDirection = Vector3.zero;
         www = new WWW(getUrl(joystickImg.rectTransform.anchoredPosition));
        
    }

    void Update () {
        timeSpent += Time.deltaTime*1000;
        getUrl(joystickImg.rectTransform.anchoredPosition);
        if (timeMax < timeSpent)
        {
            WWW www = new WWW(getUrl(joystickImg.rectTransform.anchoredPosition));
            timeSpent = 0;
        }
    }

    string getUrl(Vector3 pos) {
        Vector3 refVector = Vector3.zero;

        //Vector3.Distance(Vector3, Vector3)
        //Vector3.Angle(Vector3, Vector3)


        string urlWithParams = ""; // = url + "?speed=" + speed.ToString() + "&angle=" + angle.ToString();
        return urlWithParams;
    }
}
