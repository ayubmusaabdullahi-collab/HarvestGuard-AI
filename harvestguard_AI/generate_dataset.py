
"""
HarvestGuard AI - Nigerian Synthetic Dataset Generator
10,000 synthetic environmental records.

IMPORTANT:
This is illustrative synthetic data, NOT real Nigerian weather
observations or validated grain-spoilage measurements.
"""

import pandas as pd
import numpy as np
import random
from datetime import datetime, timedelta

# Reproducible dataset
np.random.seed(42)
random.seed(42)

RECORDS = 10000

REGIONS = {
    "Kano": {
        "season_mod": {
            "dry": (35, 20),
            "harmattan": (28, 15),
            "rainy": (29, 65)
        }
    },
    "Kaduna": {
        "season_mod": {
            "dry": (32, 25),
            "harmattan": (26, 18),
            "rainy": (26, 75)
        }
    },
    "Jos": {
        "season_mod": {
            "dry": (26, 30),
            "harmattan": (20, 20),
            "rainy": (22, 80)
        }
    },
    "Ibadan": {
        "season_mod": {
            "dry": (31, 60),
            "harmattan": (29, 50),
            "rainy": (27, 85)
        }
    },
    "Lagos": {
        "season_mod": {
            "dry": (32, 70),
            "harmattan": (30, 65),
            "rainy": (28, 88)
        }
    },
    "PortHarcourt": {
        "season_mod": {
            "dry": (31, 75),
            "harmattan": (30, 70),
            "rainy": (28, 90)
        }
    }
}

SEASONS = ["dry", "harmattan", "rainy"]

GRAIN_TYPES = [
    "maize", "rice", "millet", "sorghum", "cowpea"
]

STORAGE_TYPES = [
    "mud_silo", "metal_silo", "jute_bag", "airtight_drum"
]

STORAGE_HUMIDITY_FACTOR = {
    "mud_silo": 8,
    "metal_silo": 3,
    "jute_bag": 12,
    "airtight_drum": -5
}


def humidity_ratio(temp, relative_humidity):
    """
    Approximate humidity ratio in grams of water
    per kilogram of dry air.
    """

    saturation_pressure = 0.6108 * np.exp(
        (17.27 * temp) / (temp + 237.3)
    )

    actual_pressure = (
        saturation_pressure * relative_humidity / 100
    )

    actual_pressure = min(actual_pressure, 100.0)

    ratio = (
        0.622 * actual_pressure /
        (101.325 - actual_pressure)
    )

    return ratio * 1000


def generate_record(idx):

    # Select Nigerian location and season
    region = random.choice(list(REGIONS.keys()))

    season = random.choices(
        SEASONS,
        weights=[0.35, 0.25, 0.40]
    )[0]

    temp_base, hum_base = (
        REGIONS[region]["season_mod"][season]
    )

    # Outdoor environmental conditions
    temp_out = np.random.normal(temp_base, 2.5)
    hum_out = np.random.normal(hum_base, 7)

    temp_out = float(np.clip(temp_out, 15, 42))
    hum_out = float(np.clip(hum_out, 8, 95))

    # Storage conditions
    storage_type = random.choice(STORAGE_TYPES)

    storage_factor = STORAGE_HUMIDITY_FACTOR[storage_type]

    temp_in = (
        temp_out
        - np.random.uniform(1, 4)
        + (2 if storage_type == "metal_silo" else 0)
        + np.random.normal(0, 1.5)
    )

    hum_in = (
        hum_out
        + storage_factor
        + np.random.normal(0, 4)
    )

    temp_in = float(np.clip(temp_in, 15, 42))
    hum_in = float(np.clip(hum_in, 20, 92))

    # Calculate environmental differences
    temp_diff = temp_in - temp_out
    hum_diff = hum_in - hum_out

    # Calculate humidity ratios
    indoor_w = humidity_ratio(temp_in, hum_in)
    outdoor_w = humidity_ratio(temp_out, hum_out)

    # HarvestGuard demonstration ventilation logic
    indoor_needs_help = (
        hum_in >= 30.0 or temp_in >= 30.0
    )

    outdoor_temp_safe = (
        10.0 <= temp_out <= 35.0
    )

    outdoor_can_dry = (
        outdoor_w <= indoor_w - 1.0
    )

    outdoor_can_cool = (
        temp_out <= temp_in - 2.0
        and outdoor_w <= indoor_w + 0.5
    )

    should_ventilate = (
        indoor_needs_help
        and outdoor_temp_safe
        and (outdoor_can_dry or outdoor_can_cool)
    )

    if not indoor_needs_help:
        reason = "INDOOR_CONDITIONS_ACCEPTABLE"

    elif not outdoor_temp_safe:
        reason = "OUTDOOR_TEMP_UNSAFE"

    elif outdoor_can_dry:
        reason = "OUTDOOR_AIR_CAN_DRY"

    elif outdoor_can_cool:
        reason = "OUTDOOR_AIR_CAN_COOL"

    else:
        reason = "OUTDOOR_AIR_UNSUITABLE"

    # Synthetic illustrative risk label.
    # This is NOT a measured spoilage outcome.
    if hum_in >= 75:
        spoilage_risk = "HIGH"
    elif hum_in >= 65:
        spoilage_risk = "MEDIUM"
    else:
        spoilage_risk = "LOW"

    grain_type = random.choice(GRAIN_TYPES)

    days_stored = random.randint(1, 180)

    return {
        "record_id": idx,

        "timestamp": (
            datetime(2023, 1, 1)
            + timedelta(hours=idx * 2)
        ).isoformat(),

        "region": region,
        "season": season,
        "grain_type": grain_type,
        "storage_type": storage_type,
        "days_stored": days_stored,

        "temp_indoor": round(temp_in, 2),
        "hum_indoor": round(hum_in, 2),

        "temp_outdoor": round(temp_out, 2),
        "hum_outdoor": round(hum_out, 2),

        "temp_diff": round(temp_diff, 2),
        "hum_diff": round(hum_diff, 2),

        "should_ventilate": int(should_ventilate),
        "ventilation_reason": reason,

        "spoilage_risk": spoilage_risk,

        "is_synthetic": True,
        "data_source": "SYNTHETIC - ILLUSTRATIVE NIGERIA"
    }


# Generate all records
rows = [
    generate_record(i)
    for i in range(RECORDS)
]

df = pd.DataFrame(rows)

# Save CSV
filename = "harvestguard_nigeria_synthetic_10k.csv"

df.to_csv(filename, index=False)

# Display results
print("\n===================================")
print("HARVESTGUARD DATASET GENERATION")
print("===================================")

print(f"\nTotal records: {len(df)}")

print("\nVentilation label distribution:")
print(df["should_ventilate"].value_counts())

print("\nGrain types:")
print(df["grain_type"].value_counts())

print("\nStorage types:")
print(df["storage_type"].value_counts())

print("\nRegions:")
print(df["region"].value_counts())

print("\nFirst five records:")
print(df.head())

print("\n===================================")
print("DATASET GENERATED SUCCESSFULLY")
print(f"Saved as: {filename}")
print("IMPORTANT: SYNTHETIC DATA ONLY")
print("===================================")
