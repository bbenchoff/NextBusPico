import requests
import xml.etree.ElementTree as ET
from datetime import datetime, timedelta
import pytz

# Your API URL
url = "https://api.511.org/transit/StopMonitoring?api_key=da03f504-fc16-43e7-a736-319af37570be&agency=SF&stopCode=13565"

# Send a GET request to the URL
response = requests.get(url, headers={'Accept': 'application/xml'})

if response.status_code == 200:
    xml_data = response.content
    root = ET.fromstring(xml_data)

    # Iterate over all elements and strip namespace from tags
    for elem in root.iter():
        elem.tag = elem.tag.split('}')[-1]  # Removes namespace

    # Now, find MonitoredStopVisit elements without specifying namespace
    monitored_stop_visits = root.findall(".//MonitoredStopVisit")
    print(f"Found {len(monitored_stop_visits)} MonitoredStopVisit elements without namespace.")

    arrival_times = []
    for msv in monitored_stop_visits:
        expected_arrival_time_element = msv.find(".//ExpectedArrivalTime")
        if expected_arrival_time_element is not None:
            arrival_times.append(expected_arrival_time_element.text)
        else:
            print("No ExpectedArrivalTime element found for this MonitoredStopVisit")

    # Assuming you want the next two buses
    next_two_buses_arrival_times = arrival_times[:3]

    print("Arrival times of the next three buses:", next_two_buses_arrival_times)
else:
    print(f"Failed to retrieve data. Status code: {response.status_code}")
    print(response.text)

# Assuming the arrival times are in UTC
utc = pytz.UTC
current_time = datetime.now(utc)

# Print how far in the future the next arrival times are in minutes
for arrival_time_str in next_two_buses_arrival_times:
    # Parse the arrival time string into a datetime object
    arrival_time = datetime.strptime(arrival_time_str, "%Y-%m-%dT%H:%M:%SZ")
    arrival_time = utc.localize(arrival_time)
    
    # Calculate the difference in time
    time_diff = arrival_time - current_time
    
    # Convert the time difference to minutes
    minutes_away = time_diff.total_seconds() / 60
    print(f"Bus arriving at {arrival_time_str} is {minutes_away:.0f} minutes away.")
