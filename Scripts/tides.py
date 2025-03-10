import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import pandas as pd
from datetime import datetime
import matplotlib.ticker as ticker
import pytz
import json
import argparse
import sys

def plot_water_level_from_json(json_file, output_file=None, current_time=None, show_plot=True, 
                            min_date=None, max_date=None, args=None):
    """
    Load OFS water level data from a JSON file and plot it
    
    Parameters:
    -----------
    json_file : str
        Path to the JSON file containing water level data
    output_file : str, optional
        Path to save the chart image
    current_time : datetime, optional
        Current time to display as a vertical line
    show_plot : bool, optional
        Whether to display the plot (default: True)
    min_date : str, optional
        Filter data to only include dates >= this date (format: 'YYYY-MM-DD')
    max_date : str, optional
        Filter data to only include dates <= this date (format: 'YYYY-MM-DD')
    args : argparse.Namespace, optional
        Command line arguments
    
    Returns:
    --------
    bool
        True if successful, False if failed
    """
    # If args is None, create a default args object
    if args is None:
        class DefaultArgs:
            def __init__(self):
                self.width = 18
                self.height = 8
                self.dpi = 300
                self.tick_hours = None
                self.no_markers = False
                self.title = None
                self.debug = False
        args = DefaultArgs()
    try:
        # Check if the input is a file path or JSON string
        if json_file.endswith('.json'):
            # Load from file
            with open(json_file, 'r') as f:
                file_content = f.read()
        else:
            # Assume it's already a JSON string
            file_content = json_file
        
        # Try to fix the JSON if it's malformed (e.g., truncated at the end)
        try:
            data = json.loads(file_content)
        except json.JSONDecodeError as e:
            print(f"JSON decode error: {e}")
            print("Attempting to fix malformed JSON...")
            
            # If JSON is truncated, try to fix it by adding missing closing brackets
            if "}" not in file_content[-5:]:
                fixed_content = file_content + "}]}"
                try:
                    data = json.loads(fixed_content)
                    print("Successfully fixed JSON by adding closing brackets")
                except json.JSONDecodeError:
                    # If that didn't work, try to find the last complete data point
                    last_complete = file_content.rfind('},{"t"')
                    if last_complete > 0:
                        fixed_content = file_content[:last_complete+1] + "}]}"
                        try:
                            data = json.loads(fixed_content)
                            print("Successfully fixed JSON by truncating at last complete data point")
                        except json.JSONDecodeError as e2:
                            print(f"Could not fix JSON: {e2}")
                            return False
                    else:
                        print("Could not find a way to fix the JSON")
                        return False
            else:
                print("JSON appears to be malformed in a way that can't be automatically fixed")
                return False
        
        # Check if data is in the expected format
        if 'data' not in data:
            print("Error: Expected 'data' field not found in JSON")
            print("JSON structure:", list(data.keys()))
            return False
        
        # Extract metadata
        metadata = data.get('metadata', {})
        station_id = metadata.get('id', 'Unknown')
        station_name = metadata.get('name', 'Unknown')
        datum = metadata.get('datum', 'Unknown')
        
        # If station name/id not in metadata, try to get it from other fields
        if station_id == 'Unknown' and 'station_id' in data:
            station_id = data['station_id']
        if station_name == 'Unknown' and 'station_name' in data:
            station_name = data['station_name']
        if datum == 'Unknown' and 'datum' in data:
            datum = data['datum']
        
        # Process water level data
        df = pd.DataFrame(data['data'])
        
        print(f"Loaded {len(df)} data points from JSON")
        
        # Show the first few rows to help debug
        print("First few data points:")
        print(df.head(3))
        
        # Show columns in the dataframe
        print(f"Available columns: {df.columns.tolist()}")
        
        # Determine the format of time data
        time_col = None
        for col in ['t', 'date_time', 'time', 'DateTime']:
            if col in df.columns:
                time_col = col
                break
        
        if time_col is None:
            print("Error: Couldn't identify time column. Available columns:", df.columns.tolist())
            return False
        
        # Determine the format of value data
        value_col = None
        for col in ['v', 'value', 'water_level', 'waterLevel', 'level']:
            if col in df.columns:
                value_col = col
                break
        
        if value_col is None:
            print("Error: Couldn't identify water level column. Available columns:", df.columns.tolist())
            return False
        
        print(f"Using time column: '{time_col}' and value column: '{value_col}'")
        
        # Convert time to datetime objects
        try:
            df[time_col] = pd.to_datetime(df[time_col])
        except Exception as e:
            print(f"Error converting time column to datetime: {e}")
            print("Sample time values:", df[time_col].head(5).tolist())
            return False
            
        # Ensure values are numeric
        try:
            df[value_col] = pd.to_numeric(df[value_col], errors='coerce')
        except Exception as e:
            print(f"Error converting value column to numeric: {e}")
            print("Sample value values:", df[value_col].head(5).tolist())
            return False
            
        # Check for NaN values and drop them
        nan_count = df[value_col].isna().sum()
        if nan_count > 0:
            print(f"Warning: Found {nan_count} NaN values in the data")
            df = df.dropna(subset=[value_col])
            print(f"Dropped NaN values, {len(df)} points remaining")
        
        if df.empty:
            print("Error: No valid data points after processing")
            return False
        
        # Filter by date range if specified
        if min_date:
            min_datetime = pd.to_datetime(min_date)
            df = df[df[time_col] >= min_datetime]
            print(f"Filtered to dates >= {min_date}, {len(df)} points remaining")
            
        if max_date:
            max_datetime = pd.to_datetime(max_date)
            df = df[df[time_col] <= max_datetime]
            print(f"Filtered to dates <= {max_date}, {len(df)} points remaining")
        
        print(f"Plotting {len(df)} data points from {df[time_col].min()} to {df[time_col].max()}")
        
        # Create figure and axis with adjustable figure size
        fig, ax = plt.figure(figsize=(args.width, args.height)), plt.gca()
        
        # Plot the water level line
        ax.plot(df[time_col], df[value_col], '-', color='#0077be', linewidth=2.5)
        
        # Print extremes to help with debugging
        print(f"Data value range: {df[value_col].min()} to {df[value_col].max()}")
        
        # Create markers for specific times to make it clear all data is there
        if not args.no_markers:
            # Add markers to show the first and last 3 data points clearly
            first_3 = df.head(3)
            last_3 = df.tail(3)
            ax.plot(first_3[time_col], first_3[value_col], 'ro', markersize=5, label='First points')
            ax.plot(last_3[time_col], last_3[value_col], 'go', markersize=5, label='Last points')
            
            # Add text labels for start and end times
            ax.text(df[time_col].min(), df[value_col].max(), 
                    f"Start: {df[time_col].min().strftime('%Y-%m-%d %H:%M')}", 
                    ha='left', va='bottom', color='red', fontsize=10)
            ax.text(df[time_col].max(), df[value_col].max(), 
                    f"End: {df[time_col].max().strftime('%Y-%m-%d %H:%M')}", 
                    ha='right', va='bottom', color='green', fontsize=10)
                    
            # Add legend
            ax.legend()
        
        # Sort dataframe by time to ensure proper line plotting
        df = df.sort_values(by=time_col)
        
        # Print time range
        print(f"Data time range: {df[time_col].min()} to {df[time_col].max()}")
        print(f"Total time span: {df[time_col].max() - df[time_col].min()}")

        # Add current time vertical line if provided
        if current_time:
            ax.axvline(x=current_time, color='green', linewidth=2, label='Current Time')
            
            # Add "Current Time" text rotated along the green line
            trans = ax.get_xaxis_transform()
            ax.text(current_time, 0.98, "Current Time", rotation=90, 
                    transform=trans, va='top', ha='right', color='green', fontsize=9)
        
        # Set up the x-axis to show dates and times with more control over formatting
        # Use a formatter that explicitly shows both date and time
        date_formatter = mdates.DateFormatter('%m/%d\n%H:%M', tz=pytz.timezone('America/Los_Angeles'))
        ax.xaxis.set_major_formatter(date_formatter)
        
        # Force x-axis to show more ticks to ensure all dates are visible
        # For a 30-hour span, let's show a tick every 3 hours (10 ticks total)
        time_range = (df[time_col].max() - df[time_col].min()).total_seconds() / 3600  # in hours
        print(f"Time range is {time_range:.1f} hours")
        
        # Use user-specified tick interval if provided, otherwise calculate automatically
        if args.tick_hours:
            hours_interval = args.tick_hours
            print(f"Using user-specified tick interval of {hours_interval} hours")
        else:
            # For typical 30-hour range, use 3-hour spacing for ticks
            hours_interval = max(3, int(time_range / 10))  # At least every 3 hours, but adjust for longer spans
            print(f"Setting tick interval to {hours_interval} hours to show approximately 10 ticks")
        
        ax.xaxis.set_major_locator(mdates.HourLocator(interval=hours_interval))
        
        # Also add minor ticks for better resolution
        ax.xaxis.set_minor_locator(mdates.HourLocator(interval=1))  # Minor tick every hour
        
        # Make sure the x-axis limits match exactly with the data range
        ax.set_xlim(df[time_col].min(), df[time_col].max())
        
        # Draw vertical lines at midnight to separate days
        if time_range > 24:
            # Get all unique dates in the data
            dates = pd.to_datetime(df[time_col].dt.date.unique())
            
            # Draw vertical lines at midnight between days
            for date in dates[1:]:  # Skip the first date (we only need lines between days)
                midnight = datetime.combine(date, datetime.min.time())
                ax.axvline(midnight, color='gray', linestyle='--', alpha=0.5)
                ax.text(midnight, ax.get_ylim()[1], date.strftime('%b %d'), 
                       ha='center', va='top', fontsize=10, 
                       bbox=dict(facecolor='white', alpha=0.8, boxstyle='round'))
        
        # Set y-axis with tick marks
        y_min = df[value_col].min()
        y_max = df[value_col].max()
        y_range = y_max - y_min
        
        # Choose an appropriate tick interval
        if y_range <= 5:
            tick_interval = 0.5
        elif y_range <= 10:
            tick_interval = 1.0
        else:
            tick_interval = 2.0
        
        ax.yaxis.set_major_locator(ticker.MultipleLocator(tick_interval))
        ax.yaxis.set_minor_locator(ticker.MultipleLocator(tick_interval / 2))
        
        # Set up grid
        ax.grid(True, linestyle='-', alpha=0.7)
        
        # Set labels and title
        ax.set_xlabel('')
        ax.set_ylabel(f'Water Level in feet ({datum})', fontsize=10)
        
        # Calculate time range for title
        begin_time = df[time_col].min()
        end_time = df[time_col].max()
        
        # Use a more descriptive title that shows exactly what's plotted
        if 'title' in locals() and title:
            title_text = title
        else:
            title_text = (f"NOAA/NOS/CO-OPS Water Level Data\n"
                         f"Station {station_id}, {station_name}\n"
                         f"From {begin_time.strftime('%Y-%m-%d %H:%M')} to "
                         f"{end_time.strftime('%Y-%m-%d %H:%M')} ({len(df)} points, {datum} datum)")
        
        plt.title(title_text, fontsize=12)
        
        # Set y-axis limits with some padding
        padding = max(0.5, y_range * 0.1)  # At least 0.5 feet or 10% of range
        plt.ylim(y_min - padding, y_max + padding)
        
        # Add a small credit at the bottom right
        ax.text(0.99, 0.01, "NOAA/NOS/Center for Operational Oceanographic Products and Services",
                transform=ax.transAxes, fontsize=7, ha='right', va='bottom')
        
        # Remove top and right spines
        ax.spines['top'].set_visible(False)
        ax.spines['right'].set_visible(False)
        
        # Set tight layout
        plt.tight_layout()
        
        # Save the figure if output file is provided
        if output_file:
            plt.savefig(output_file, dpi=args.dpi, bbox_inches='tight')
            print(f"Water level chart saved as {output_file} (DPI: {args.dpi})")
            
            # Create a data statistics file alongside the image
            stats_file = output_file.rsplit('.', 1)[0] + '_stats.txt'
            with open(stats_file, 'w') as f:
                f.write(f"WATER LEVEL DATA STATISTICS\n")
                f.write(f"{'='*30}\n\n")
                f.write(f"Total data points: {len(df)}\n")
                f.write(f"Time range: {df[time_col].min()} to {df[time_col].max()}\n")
                f.write(f"Duration: {df[time_col].max() - df[time_col].min()}\n")
                f.write(f"Value range: {df[value_col].min():.2f} to {df[value_col].max():.2f} feet\n\n")
                
                # Count points per day
                f.write("POINTS PER DAY\n")
                f.write(f"{'-'*20}\n")
                days = df[time_col].dt.date.value_counts().sort_index()
                for day, count in days.items():
                    f.write(f"{day}: {count} points\n")
                
            print(f"Data statistics saved to {stats_file}")
        
        # Show the plot if requested
        if show_plot:
            plt.show()
            
        return True
        
    except Exception as e:
        print(f"Error plotting water level chart: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(description='Plot OFS water level data from JSON file')
    
    # Required arguments
    parser.add_argument('json_file', type=str, help='Path to the JSON file or raw JSON string')
    
    # Optional arguments
    parser.add_argument('--output', '-o', type=str, help='Output file path for the chart (PNG image)')
    parser.add_argument('--current-date', type=str, help='Current date in YYYYMMDD format')
    parser.add_argument('--current-hour', type=int, help='Current hour (0-23)')
    parser.add_argument('--save-only', action='store_true', help='Save the chart without displaying it')
    parser.add_argument('--min-date', type=str, help='Filter data to only include dates >= this date (YYYY-MM-DD)')
    parser.add_argument('--max-date', type=str, help='Filter data to only include dates <= this date (YYYY-MM-DD)')
    parser.add_argument('--fix-json', action='store_true', help='Attempt to fix malformed JSON')
    parser.add_argument('--debug', action='store_true', help='Print additional debug information')
    parser.add_argument('--dpi', type=int, default=300, help='DPI for saved chart (default: 300)')
    parser.add_argument('--width', type=float, default=18, help='Figure width in inches (default: 18)')
    parser.add_argument('--height', type=float, default=8, help='Figure height in inches (default: 8)')
    parser.add_argument('--tick-hours', type=int, help='Hours between x-axis ticks (default: auto)')
    parser.add_argument('--no-markers', action='store_true', help='Hide start/end markers')
    
    args = parser.parse_args()
    
    # Enable debug mode if requested
    if args.debug:
        print("Debug mode enabled")
        print(f"Input file/string: {args.json_file[:100]}...")
        
    # Set up current time if specified
    current_time = None
    if args.current_date:
        try:
            current_year = int(args.current_date[0:4])
            current_month = int(args.current_date[4:6])
            current_day = int(args.current_date[6:8])
            current_hour = args.current_hour if args.current_hour is not None else 12
            current_time = datetime(current_year, current_month, current_day, current_hour, 0)
            print(f"Using current time reference: {current_time}")
        except Exception as e:
            print(f"Error parsing current date/time: {e}")
            print("Proceeding without current time indicator")
    
    # If input is a raw JSON string and fix-json is enabled, try to fix it
    json_input = args.json_file
    if args.fix_json and not args.json_file.endswith('.json'):
        # Check if it looks like JSON
        if not json_input.strip().startswith('{'):
            json_input = '{' + json_input
            print("Added opening bracket to JSON input")
        if not json_input.strip().endswith('}'):
            json_input = json_input + '}'
            print("Added closing bracket to JSON input")
        print("Attempted to fix raw JSON input")
    
    # If no output file specified but save-only is set, create a default filename
    if args.save_only and not args.output:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        args.output = f"water_level_chart_{timestamp}.png"
        print(f"No output file specified with --save-only, using default: {args.output}")
    
    # Plot water level chart - we won't filter by date to ensure we plot all data
    success = plot_water_level_from_json(
        json_file=json_input,
        output_file=args.output,
        current_time=current_time,
        show_plot=not args.save_only,
        min_date=None,  # Don't filter by min date
        max_date=None,  # Don't filter by max date
        args=args       # Pass all args to the plotting function
    )
    
    if not success:
        sys.exit(1)

# Simple example of manually creating a JSON file with tide data
def create_example_json_file(filename="example_tide_data.json"):
    """Create an example JSON file with tide data"""
    import json
    import random
    from datetime import datetime, timedelta
    
    # Create sample data
    start_date = datetime(2025, 2, 28)
    data = []
    
    # Generate 48 hours of data at 6-minute intervals
    for i in range(48 * 10):  # 48 hours with 10 points per hour
        time_point = start_date + timedelta(minutes=6*i)
        # Create a sine wave pattern with some randomness
        hour_of_day = time_point.hour + time_point.minute/60
        # Tide cycle is roughly 12 hours
        tide_value = 3 + 2 * math.sin(hour_of_day * math.pi / 6) + random.uniform(-0.2, 0.2)
        
        data.append({
            "t": time_point.strftime("%Y-%m-%d %H:%M"),
            "v": f"{tide_value:.3f}"
        })
    
    # Create JSON structure
    json_data = {
        "metadata": {
            "id": "9414290",
            "name": "San Francisco, CA",
            "datum": "NAVD"
        },
        "data": data
    }
    
    # Write to file
    with open(filename, 'w') as f:
        json.dump(json_data, f, indent=2)
    
    print(f"Created example file with {len(data)} data points: {filename}")
    return filename

if __name__ == "__main__":
    # If no arguments are provided, show a simple demo
    if len(sys.argv) == 1:
        print("No arguments provided. Running demo with sample data...")
        example_file = create_example_json_file()
        plot_water_level_from_json(example_file)
        sys.exit(0)
    
    main()