import requests
from ultralytics import YOLO
from collections import Counter
import cv2
import time
import threading
import lgpio

# ================== GPIO ==================
LED_PIN = 26
h = lgpio.gpiochip_open(0)
lgpio.gpio_claim_output(h, LED_PIN)
lgpio.gpio_write(h, LED_PIN, 0)

# ================== SERVER ==================
SERVER_URL = "http://54.153.160.152:8000/read_camera"
DETECT_URL = "http://54.153.160.152:8000/detect"
UPLOAD_URL = "http://54.153.160.152:8000/upload_image"
BUTTON_URL = "http://54.153.160.152:8000/read_button"

# ================== YOLO ==================
model = YOLO("/home/pi/best.pt")

# ================== LED STATE ==================
led_lock = threading.Lock()
led_mode = "AUTO"   # AUTO | YOLO

# ================== LED FUNCTIONS ==================
def led_on():
    with led_lock:
        lgpio.gpio_write(h, LED_PIN, 1)

def led_off():
    with led_lock:
        lgpio.gpio_write(h, LED_PIN, 0)

# ================== UPLOAD IMAGE ==================
def upload_image(image_path):
    try:
        with open(image_path, "rb") as f:
            files = {"file": f}
            r = requests.post(UPLOAD_URL, files=files, timeout=10)

        if r.status_code == 200:
            print("Upload OK")
        else:
            print("Upload FAIL:", r.status_code)

    except Exception as e:
        print("Upload error:", e)

# ================== SEND DETECT ==================
def send_detect(results):
    objects = []

    for r in results:
        if r.boxes is None or len(r.boxes) == 0:
            continue
        for c in r.boxes.cls.tolist():
            objects.append(r.names[int(c)])

    if not objects:
        payload = {"Khong phat hien": 1}
    else:
        payload = dict(Counter(objects))

    try:
        requests.post(DETECT_URL, json=payload, timeout=5)
        print("Detect sent:", payload)
    except Exception as e:
        print("Detect error:", e)

# ================== YOLO FUNCTION ==================
def run_yolo():
    global led_mode
    print("YOLO start")

    led_mode = "YOLO"
    led_on()
    time.sleep(0.3)

    cap = cv2.VideoCapture(0)
    ret, frame = cap.read()
    cap.release()

    led_off()
    led_mode = "AUTO"

    if not ret:
        print("Camera read failed")
        return

    results = model.predict(source=frame, device="cpu", conf=0.25, save=False)
    send_detect(results)

    for i, r in enumerate(results):
        r.boxes = r.boxes[r.boxes.conf > 0.5]
        img = r.plot()
        path = f"/home/pi/picture{i}.jpg"
        cv2.imwrite(path, img)
        upload_image(path)

    print("YOLO done")

# ================== THREAD: BUTTON ==================
def thread_button():
    global led_mode
    print("Button thread started")

    while True:
        try:
            r = requests.get(BUTTON_URL, timeout=3)
            if r.status_code == 200:
                data = r.json().get("data")
                if data and led_mode == "AUTO":
                    if data.get("button6") == 0 and data.get("button7") == 1:
                        led_on()
                    else:
                        led_off()
            time.sleep(0.2)

        except Exception as e:
            print("Button thread error:", e)
            time.sleep(1)

# ================== THREAD: CAMERA ==================
def thread_camera():
    print("Camera thread started")
    last_command = None

    while True:
        try:
            r = requests.get(SERVER_URL, timeout=5)
            if r.status_code == 200:
                command = r.json().get("data", {}).get("camera")
                if command == 1 and command != last_command:
                    run_yolo()
                last_command = command
            time.sleep(0.3)

        except Exception as e:
            print("Camera thread error:", e)
            time.sleep(1)

# ================== MAIN ==================
def main():
    print("System starting...")

    t_btn = threading.Thread(target=thread_button, daemon=True)
    t_cam = threading.Thread(target=thread_camera, daemon=True)

    t_btn.start()
    t_cam.start()

    while True:
        time.sleep(1)

# ================== ENTRY ==================
if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("Exit program")
    finally:
        lgpio.gpiochip_close(h)
