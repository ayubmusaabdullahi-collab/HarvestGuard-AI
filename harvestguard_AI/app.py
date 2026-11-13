
from flask import Flask, request, jsonify
from flask_cors import CORS
import joblib
import pandas as pd
import numpy as np
import os

app = Flask(__name__)
CORS(app)

# Load trained HarvestGuard AI files
try:
    model = joblib.load("model_ventilation_rf.pkl")
    scaler = joblib.load("scaler.pkl")

    le_region = joblib.load("le_region.pkl")
    le_season = joblib.load("le_season.pkl")
    le_grain = joblib.load("le_grain.pkl")
    le_storage = joblib.load("le_storage.pkl")

    MODEL_LOADED = True
    print("HarvestGuard AI model loaded successfully.")

except Exception as error:
    MODEL_LOADED = False
    print("AI MODEL ERROR:", error)


@app.route("/health", methods=["GET"])
def health():

    return jsonify({
        "status": "online",
        "model_loaded": MODEL_LOADED,
        "system": "HarvestGuard AI",
        "ai_role": "Advisory only. ESP32 controls safety."
    })


@app.route("/predict", methods=["POST"])
def predict():

    if not MODEL_LOADED:
        return jsonify({
            "error": "AI model is not loaded."
        }), 503

    try:
        data = request.get_json()

        if not data:
            return jsonify({
                "error": "No sensor data received."
            }), 400

        temp_in = float(data["tempIn"])
        hum_in = float(data["humIn"])
        temp_out = float(data["tempOut"])
        hum_out = float(data["humOut"])

        region = data.get("region", "Jos")
        season = data.get("season", "rainy")
        grain = data.get("grain_type", "maize")
        storage = data.get("storage_type", "jute_bag")
        days_stored = float(data.get("days_stored", 30))

        # Encode categorical inputs
        def encode_value(encoder, value):
            value = str(value)

            if value not in encoder.classes_:
                raise ValueError(
                    f"Unknown category: {value}"
                )

            return int(encoder.transform([value])[0])

        # Calculate model features
        features = {
            "temp_indoor": temp_in,
            "hum_indoor": hum_in,
            "temp_outdoor": temp_out,
            "hum_outdoor": hum_out,
            "temp_diff": temp_in - temp_out,
            "hum_diff": hum_in - hum_out,
            "hum_ratio": hum_in / (hum_out + 0.001),
            "days_stored": days_stored,

            "region_enc": encode_value(
                le_region, region
            ),

            "season_enc": encode_value(
                le_season, season
            ),

            "grain_enc": encode_value(
                le_grain, grain
            ),

            "storage_enc": encode_value(
                le_storage, storage
            ),

            "heat_index_in": temp_in + 0.5555 * (
                6.11 * np.exp(
                    (17.27 * temp_in) /
                    (237.7 + temp_in)
                ) * hum_in / 100 - 10
            )
        }

        # Prepare input in the same order as training
        columns = [
            "temp_indoor",
            "hum_indoor",
            "temp_outdoor",
            "hum_outdoor",
            "temp_diff",
            "hum_diff",
            "hum_ratio",
            "days_stored",
            "region_enc",
            "season_enc",
            "grain_enc",
            "storage_enc",
            "heat_index_in"
        ]

        X = pd.DataFrame(
            [[features[column] for column in columns]],
            columns=columns
        )

        X_scaled = scaler.transform(X)

        prediction = int(model.predict(X_scaled)[0])
        probabilities = model.predict_proba(X_scaled)[0]

        confidence = float(np.max(probabilities))

        # Advisory result only.
        # ESP32 retains final control of the fan.
        if prediction == 1:
            recommendation = (
                "AI suggests ventilation. "
                "Check ESP32 safety conditions before operation."
            )
        else:
            recommendation = (
                "AI suggests holding ventilation "
                "and continuing environmental monitoring."
            )

        return jsonify({
            "system": "HarvestGuard AI",
            "should_ventilate": bool(prediction),
            "confidence": round(confidence * 100, 2),
            "recommendation": recommendation,
            "safety_authority": "ESP32",
            "disclaimer": (
                "Model trained using synthetic data. "
                "Real-world validation is required."
            )
        })

    except Exception as error:

        return jsonify({
            "error": str(error)
        }), 400


if __name__ == "__main__":

    print("\n==============================")
    print("HARVESTGUARD AI SERVER")
    print("==============================")

    app.run(
        host="0.0.0.0",
        port=5000,
        debug=False
    )
