
"""
HarvestGuard AI - RandomForest Training Program

Trains a ventilation recommendation model using
the synthetic Nigerian environmental dataset.

IMPORTANT:
Synthetic training results are not proof of
real-world grain-storage or spoilage accuracy.
"""

import os
import joblib
import pandas as pd
import numpy as np

from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler, LabelEncoder
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import (
    accuracy_score,
    classification_report,
    confusion_matrix
)

# ------------------------------------------------
# 1. FILE CONFIGURATION
# ------------------------------------------------

DATA_FILE = "harvestguard_nigeria_synthetic_10k.csv"

# ------------------------------------------------
# 2. LOAD DATASET
# ------------------------------------------------

print("\nLoading HarvestGuard dataset...")

if not os.path.exists(DATA_FILE):
    raise FileNotFoundError(
        f"Dataset not found: {DATA_FILE}"
    )

df = pd.read_csv(DATA_FILE)

print("Dataset loaded successfully.")
print("Records:", len(df))

if len(df) < 100:
    raise ValueError("Dataset is too small for training.")

if "is_synthetic" not in df.columns:
    raise ValueError("Synthetic dataset marker is missing.")

if not df["is_synthetic"].astype(str).str.lower().isin(
    ["true", "1"]
).all():
    raise ValueError("Unexpected non-synthetic records found.")

# ------------------------------------------------
# 3. CHECK TARGET
# ------------------------------------------------

TARGET = "should_ventilate"

if TARGET not in df.columns:
    raise ValueError("Target column is missing.")

print("\nTarget distribution:")
print(df[TARGET].value_counts())

if df[TARGET].nunique() != 2:
    raise ValueError(
        "Training requires both ventilation classes."
    )

# ------------------------------------------------
# 4. ENCODE CATEGORICAL FEATURES
# ------------------------------------------------

print("\nEncoding categorical features...")

le_region = LabelEncoder()
le_season = LabelEncoder()
le_grain = LabelEncoder()
le_storage = LabelEncoder()

df["region_enc"] = le_region.fit_transform(
    df["region"].astype(str)
)

df["season_enc"] = le_season.fit_transform(
    df["season"].astype(str)
)

df["grain_enc"] = le_grain.fit_transform(
    df["grain_type"].astype(str)
)

df["storage_enc"] = le_storage.fit_transform(
    df["storage_type"].astype(str)
)

# ------------------------------------------------
# 5. FEATURE ENGINEERING
# ------------------------------------------------

print("Engineering model features...")

df["temp_diff"] = (
    df["temp_indoor"] - df["temp_outdoor"]
)

df["hum_diff"] = (
    df["hum_indoor"] - df["hum_outdoor"]
)

# Keep this formula consistent with the current
# feature calculation in app.py.
df["hum_ratio"] = (
    df["hum_indoor"] /
    (df["hum_outdoor"] + 0.001)
)

temp_in = df["temp_indoor"]
hum_in = df["hum_indoor"]

df["heat_index_in"] = temp_in + 0.5555 * (
    6.11 * np.exp(
        (17.27 * temp_in) / (237.7 + temp_in)
    ) * hum_in / 100 - 10
)

# IMPORTANT:
# Keep this order identical in app.py.
FEATURE_COLUMNS = [
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

X = df[FEATURE_COLUMNS].copy()
y = df[TARGET].astype(int)

if not np.isfinite(X.to_numpy()).all():
    raise ValueError("Invalid numeric values found in features.")

# ------------------------------------------------
# 6. SPLIT TRAINING AND TEST DATA
# ------------------------------------------------

print("\nSplitting dataset...")

X_train, X_test, y_train, y_test = train_test_split(
    X,
    y,
    test_size=0.20,
    random_state=42,
    stratify=y
)

print("Training records:", len(X_train))
print("Testing records:", len(X_test))

# ------------------------------------------------
# 7. SCALE FEATURES
# ------------------------------------------------

scaler = StandardScaler()

X_train_scaled = scaler.fit_transform(X_train)
X_test_scaled = scaler.transform(X_test)

# ------------------------------------------------
# 8. TRAIN RANDOM FOREST
# ------------------------------------------------

print("\nTraining RandomForest model...")

model = RandomForestClassifier(
    n_estimators=200,
    max_depth=12,
    min_samples_split=4,
    min_samples_leaf=2,
    random_state=42,
    class_weight="balanced",
    n_jobs=-1
)

model.fit(X_train_scaled, y_train)

print("Training completed.")

# ------------------------------------------------
# 9. EVALUATE MODEL
# ------------------------------------------------

print("\nEvaluating model...")

predictions = model.predict(X_test_scaled)

accuracy = accuracy_score(y_test, predictions)

print("\nTest accuracy:", round(accuracy * 100, 2), "%")

print("\nClassification report:")
print(
    classification_report(
        y_test,
        predictions,
        target_names=["HOLD", "VENTILATE"],
        zero_division=0
    )
)

print("\nConfusion matrix:")
print(confusion_matrix(y_test, predictions))

# ------------------------------------------------
# 10. SAVE MODEL AND ENCODERS
# ------------------------------------------------

print("\nSaving model files...")

joblib.dump(model, "model_ventilation_rf.pkl")
joblib.dump(scaler, "scaler.pkl")

joblib.dump(le_region, "le_region.pkl")
joblib.dump(le_season, "le_season.pkl")
joblib.dump(le_grain, "le_grain.pkl")
joblib.dump(le_storage, "le_storage.pkl")

print("\n===================================")
print("HARVESTGUARD AI TRAINING COMPLETED")
print("===================================")

print("Generated files:")

files = [
    "model_ventilation_rf.pkl",
    "scaler.pkl",
    "le_region.pkl",
    "le_season.pkl",
    "le_grain.pkl",
    "le_storage.pkl"
]

for filename in files:
    print(" -", filename)

print("\nIMPORTANT:")
print("Model trained using SYNTHETIC data.")
print("Real-world validation is still required.")
