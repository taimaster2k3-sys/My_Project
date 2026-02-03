import uvicorn
import os
import shutil
from fastapi import FastAPI, HTTPException, Response, UploadFile, File, APIRouter, Query
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
import pymongo
from datetime import datetime, timedelta
from zoneinfo import ZoneInfo
from typing import Dict
from fastapi.staticfiles import StaticFiles

app = FastAPI()

router = APIRouter()
IMAGE_ROOT = "images"


os.makedirs("images", exist_ok=True)
app.mount("/images", StaticFiles(directory="images"), name="images")
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

myclient = pymongo.MongoClient("mongodb+srv://lylancook95:dotantai@demo.gxjmg.mongodb.net/?retryWrites=true&w=majority&appName=demo")
mydb = myclient["mydatabase"]
sensor = mydb["SENSOR"]
button = mydb["BUTTON"]
camera = mydb["CAMERA"]
detect = mydb["DETECT"]
device_status = mydb["DEVICE_STATUS"]
counter_col = mydb["counters"]
camera_image = mydb["CAMERA_IMAGE"]

class SENSOR(BaseModel):
    temperature: float
    humidity: float
    CO2: float
class BUTTON(BaseModel):
    button1: int
    button2: int
    button3: int
    button4: int
    button5: int
    button6: int
    button7: int

class CAMERA(BaseModel):
    camera: int

class DEVICE_STATUS(BaseModel):
    fan1_duty: int
    fan2_duty: int
    humidifier: str
    heating: str

def get_next_id(counter_name: str):
    counter = counter_col.find_one_and_update(
        {"_id": counter_name},
        {"$inc": {"sequence_value": 1}},
        upsert=True,
        return_document=pymongo.ReturnDocument.AFTER
    )
    return counter["sequence_value"]

def time_vn():
    return datetime.now(ZoneInfo("Asia/Ho_Chi_Minh"))

@app.post("/reset_data")
async def reset_data():
    try:
        sensor.delete_many({})
        counter_col.delete_many({})
        button.delete_many({})
        camera.delete_many({})
        detect.delete_many({})
        device_status.delete_many({})
        camera_image.delete_many({})
        counter_col.update_many({},{"$set": {"sequence_value": 0}})
        return {"status": "success"}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@router.delete("/reset_images")
def reset_images(key: str = Query(...)):
    if key != "RESET123":
        raise HTTPException(status_code=403, detail="Forbidden")

    if os.path.exists(IMAGE_ROOT):
        shutil.rmtree(IMAGE_ROOT)
        os.makedirs(IMAGE_ROOT)

    camera_image.delete_many({})

    return {
        "status": "success",
        "message": "Đã reset toàn bộ ảnh trên server và metadata trên MongoDB Atlas"
    }
app.include_router(router)

@app.post("/upload_image")
async def upload_image(file: UploadFile = File(...)):
    try:
        next_id = get_next_id("camera_image_id")

        date_folder = now.strftime("%Y-%m-%d")
        save_dir = f"images/{date_folder}"
        os.makedirs(save_dir, exist_ok=True)

        filename = f"img_{now.strftime('%H%M%S')}_{file.filename}"
        file_path = f"{save_dir}/{filename}"

        with open(file_path, "wb") as f:
            f.write(await file.read())

        doc = {
            "_id": next_id,
            "filename": filename,
            "path": file_path,
            "timestamp": time_vn(),
            "source": "raspberry_pi"
        }

        camera_image.insert_one(doc)

        return {
            "status": "success",
            "data": doc
        }

    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))


@app.post("/status_device")
async def post_device_status(data: DEVICE_STATUS):
    try:
        next_id = get_next_id("device_status_id")

        doc = {
            "_id": next_id,
            "fan1_duty": data.fan1_duty,
            "fan2_duty": data.fan2_duty,
            "humidifier": data.humidifier,
            "heating": data.heating,
            "timestamp": time_vn()
        }

        device_status.insert_one(doc)

        return {
            "status": "success",
            "data": doc
        }

    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))


@app.post("/write_data")
async def receive_sensor_data(data: SENSOR):
    try:
        next_id = get_next_id("sensor_id")
        Data = {
            "_id": next_id,
            "temperature": data.temperature,
            "humidity": data.humidity,
            "CO2": data.CO2,
            "timestamp": time_vn()
        }
        sensor.insert_one(Data)
        return {"status": "success", "data": Data}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/button_all")
async def button_all(data: BUTTON):
    try:
        next_id = get_next_id("button_id")
        doc = {
            "_id": next_id,
            "button1": int(data.button1),
            "button2": int(data.button2),
            "button3": int(data.button3),
            "button4": int(data.button4),
            "button5": int(data.button5),
            "button6": int(data.button6),
            "button7": int(data.button7),
            "time": time_vn()
        }
        button.insert_one(doc)
        return {"status": "success", "data": doc}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))


@app.get("/read_data")
async def read_data():
    try:
        dulieu = sensor.find_one(sort=[("_id", -1)])
        if dulieu:
            response = {
                "status": "success",
                "data": {
                    "temperature": dulieu["temperature"],
                    "humidity": dulieu["humidity"],
                    "CO2": dulieu["CO2"],
                    "timestamp": dulieu["timestamp"]
                }
            }
            return response
        else:
            return {"status": "success", "data": None}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/read_button")
async def read_button():
    try:
        dulieu = button.find_one(sort=[("_id", -1)])
        if dulieu:
            response = {
                "status": "success",
                "data": {
                    "button1": dulieu["button1"],
                    "button2": dulieu["button2"],
                    "button3": dulieu["button3"],
                    "button4": dulieu["button4"],
                    "button5": dulieu["button5"],
                    "button6": dulieu["button6"],
                    "button7": dulieu["button7"],
                    "time": dulieu["time"]
                }
            }
            return response
        else:
            return {"status": "success", "data": None}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/camera")
async def camera_cmd(data: CAMERA):
    last = camera.find_one(sort=[("_id", -1)])
    last_state = last["camera"] if last else None

    if last_state != data.camera:
        next_id = get_next_id("camera_id")
        doc = {
            "_id": next_id,
            "camera": data.camera,
            "timestamp": time_vn()
        }
        camera.insert_one(doc)
        return {"status": "success", "data": doc}
    else:
        return {"status": "ignored", "data": {"camera": data.camera}}

@app.get("/read_camera")
async def get_latest_camera_command():
    try:
        latest_doc = camera.find_one(sort=[("_id", -1)])
        if latest_doc:
            response = {
                "status": "success",
                "data": {
                    "camera": latest_doc["camera"],
                    "timestamp": latest_doc["timestamp"]
                }
            }
            return response
        else:
            return {"status": "success", "data": None}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/detect")
async def receive_detect(data: Dict[str, int]):
    try:
        next_id = get_next_id("detect_id")

        doc = {
            "_id": next_id,
            "objects": data,
            "timestamp": time_vn()
        }

        detect.insert_one(doc)

        return {
            "status": "success",
            "data": doc
        }

    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/read_detect")
async def read_detect():
    try:
        doc = detect.find_one(sort=[("_id", -1)])

        if doc:
            return {
                "status": "success",
                "data": {
                    "objects": doc.get("objects", {}),
                    "timestamp": doc.get("timestamp")
                }
            }
        else:
            return {
                "status": "success",
                "data": None
            }

    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/read_status_device")
async def read_status_device():
    try:
        doc = device_status.find_one(sort=[("_id", -1)])
        if doc:
            return {"status": "success", "data": doc}
        return {"status": "success", "data": None}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/read_bieudo")
async def read_bieudo_3h():
    try:
        now_vn = time_vn()
        three_hours_ago = now_vn - timedelta(hours=3)
        pipeline = [
                {"$match": {"timestamp": {"$gte": three_hours_ago}}},
                {"$group": {"_id": {"$dateTrunc": {"date": "$timestamp", "unit": "minute"}}, "doc": {"$first": "$$ROOT"}}},
                {"$replaceRoot": {"newRoot": "$doc"}},
                {"$addFields": {"timestamp_vn": {"$dateAdd": {"startDate": "$timestamp", "unit": "hour", "amount": 7}}}},
                {"$sort": {"timestamp_vn": 1}}
                ]

        data = list(sensor.aggregate(pipeline))

        return {
            "status": "success",
            "from": three_hours_ago,
            "to": now_vn,
            "count": len(data),
            "data": data
        }

    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/read_images")
async def read_images(limit: int = 20):
    try:
        data = list(
            camera_image.find()
            .sort("_id", -1)
            .limit(limit)
        )
        return {
            "status": "success",
            "count": len(data),
            "data": data
        }
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/status")
async def get_status():
    return {"status": "connected"}

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)
