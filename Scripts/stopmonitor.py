#!/usr/bin/env python3
"""
Real‑time 511 SFMTA stop monitor

• Polls the 511 Siri StopMonitoring endpoint once every 60 s  
• Logs every MonitoredStopVisit it sees: time polled, route, destination, expected arrival (ISO UTC), minutes‑away  
• Prints the same information to the console in a human‑friendly format

Save as `stop_monitor.py`, set your API key, then run:

    python stop_monitor.py
"""

import requests, json, time
from json import JSONDecodeError
from datetime import datetime, timezone
from pathlib import Path

# ── USER SETTINGS ────────────────────────────────────────────────────────────
API_KEY   = "da03f504-fc16-43e7-a736-319af37570be"   # ←← <<< PUT KEY HERE
STOP_CODE = "16633"                                  # Muni stop ID
AGENCY    = "SF"                                     # 511 agency tag for Muni
POLL_SECS = 62                                       # how often to hit the API
CSV_FILE  = Path(f"stop_{STOP_CODE}_arrivals.csv")
JSONL_FILE = Path(f"stop_{STOP_CODE}_raw.jsonl")
# ──────────────────────────────────────────────────────────────────────────────

URL = ( "https://api.511.org/transit/StopMonitoring"
        f"?api_key={API_KEY}&agency={AGENCY}&stopCode={STOP_CODE}&format=json" )

def ensure_files():
    if not CSV_FILE.exists():
        CSV_FILE.write_text(
            "record,polled_utc,line,destination,arrival_utc,minutes_away\n",
            encoding="utf‑8"
        )
    JSONL_FILE.touch(exist_ok=True)

def bom_safe_json(resp: requests.Response):
    """Return parsed JSON, tolerating an optional UTF‑8 BOM."""
    try:
        return resp.json()
    except Exception:
        txt = resp.content.decode("utf‑8-sig", errors="replace").lstrip("\ufeff")
        return json.loads(txt)

def minutes_between(later: datetime, earlier: datetime) -> int:
    return int((later - earlier).total_seconds() // 60)

def parse_visits(payload: dict):
    """
    Yield (line, destination, arrival_datetime_utc) tuples for every
    MonitoredStopVisit in the payload.  Works whether the response has a
    top‑level 'Siri' key or not, and whether StopMonitoringDelivery is a list
    or a single dict.
    """
    # 1) Locate ServiceDelivery
    if "Siri" in payload:
        sd = payload["Siri"]["ServiceDelivery"]
    elif "ServiceDelivery" in payload:          # what you’re seeing
        sd = payload["ServiceDelivery"]
    else:
        return                                  # nothing usable

    # 2) Grab (first) StopMonitoringDelivery
    smd = sd.get("StopMonitoringDelivery")
    if smd is None:
        return
    if isinstance(smd, list):
        smd = smd[0]                            # take first element

    visits = smd.get("MonitoredStopVisit", [])
    if not isinstance(visits, list):
        visits = [visits]

    # 3) Extract the useful bits
    for v in visits:
        try:
            mvj = v["MonitoredVehicleJourney"]

            # Line
            line = mvj.get("PublishedLineName") or mvj.get("LineRef", "UNKNOWN")
            if isinstance(line, list):
                line = line[0].get("value", "UNKNOWN")
            elif isinstance(line, dict):
                line = line.get("value", "UNKNOWN")

            # Destination
            dest = mvj.get("DestinationName") or mvj.get("DestinationRef", "UNKNOWN")
            if isinstance(dest, list):
                dest = dest[0].get("value", "UNKNOWN")
            elif isinstance(dest, dict):
                dest = dest.get("value", "UNKNOWN")

            # Expected arrival (UTC)
            arr_dt = datetime.fromisoformat(
                mvj["MonitoredCall"]["ExpectedArrivalTime"]
                .replace("Z", "+00:00")
            ).astimezone(timezone.utc)

            yield line.strip(), dest.strip(), arr_dt
        except Exception:
            continue
        
def log_csv(csv_fh, rec, polled_utc, line, dest, arr_utc, mins):
    csv_fh.write(
        f"{rec},{polled_utc.isoformat(timespec='seconds')},"
        f"{line},{dest},{arr_utc.isoformat(timespec='seconds')},{mins}\n"
    )
    csv_fh.flush()

def append_jsonl(raw_bytes: bytes, polled_utc: datetime):
    """Append one JSON‑Lines record to the archive file."""
    record = {
        "polled_utc": polled_utc.isoformat(timespec="seconds"),
        "payload": json.loads(
            raw_bytes.decode("utf‑8-sig", errors="replace").lstrip("\ufeff")
        ),
    }
    with JSONL_FILE.open("a", encoding="utf‑8") as fh:
        json.dump(record, fh)
        fh.write("\n")

# ----- main loop ------------------------------------------------------------
def main():
    ensure_files()
    print(f"Archiving raw replies → {JSONL_FILE.resolve()}")
    print(f"Arrival CSV          → {CSV_FILE.resolve()}\nCtrl‑C to stop.\n")

    record_no = 1
    with CSV_FILE.open("a", encoding="utf‑8") as csv_fh:
        try:
            while True:
                polled_utc = datetime.now(timezone.utc)

                try:
                    resp = requests.get(URL, timeout=15)
                    resp.raise_for_status()
                except requests.RequestException as e:
                    print(f"Request failed: {e}")
                    time.sleep(POLL_SECS)
                    continue

                append_jsonl(resp.content, polled_utc)

                data = bom_safe_json(resp)
                visits = list(parse_visits(data))
                if not visits:
                    print(f"[{polled_utc.astimezone():%H:%M:%S}]  -- no visits --")

                for line, dest, arr_utc in visits:
                    mins = minutes_between(arr_utc, polled_utc)
                    print(f"[{polled_utc.astimezone():%H:%M:%S}] "
                          f"{line:>4} to {dest:<25} → {mins:>3} min")
                    log_csv(csv_fh, record_no, polled_utc, line, dest, arr_utc, mins)
                    record_no += 1

                time.sleep(POLL_SECS)
        except KeyboardInterrupt:
            print("\nStopped by user.")

if __name__ == "__main__":
    main()