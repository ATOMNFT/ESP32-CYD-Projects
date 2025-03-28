from flask import Flask, jsonify
import psutil
import time
import json

app = Flask(__name__)

def get_stats():
    try:
        boot_time = psutil.boot_time()
        current_time = time.time()
        uptime_seconds = max(0, current_time - boot_time)
        
        uptime_hours = int(uptime_seconds // 3600)
        uptime_minutes = int((uptime_seconds % 3600) // 60)
        uptime_seconds = int(uptime_seconds % 60)
        
        cpu_temp = "N/A"
        temperatures = psutil.sensors_temperatures()
        if "coretemp" in temperatures:
            cpu_temp = temperatures["coretemp"][0].current
        elif "cpu_thermal" in temperatures:
            cpu_temp = temperatures["cpu_thermal"][0].current

        stats = {
            "cpu": psutil.cpu_percent(interval=1),
            "memory": psutil.virtual_memory().percent,
            "disk": psutil.disk_usage('/').percent,
            "uptime": f"{uptime_hours}:{uptime_minutes:02}:{uptime_seconds:02}",
            "cpu_temp": str(cpu_temp)
        }

        # Save stats to file only if needed, or conditionally
        with open("stats.json", "w") as json_file:
            json.dump(stats, json_file, indent=4)
        
        return stats

    except psutil.Error as e:
        return {"error": str(e)}

@app.route('/stats', methods=['GET'])
def serve_stats():
    return jsonify(get_stats())

if __name__ == "__main__":
    app.run(host='0.0.0.0', port=5000)  # Edit port to use another. Port must match entry in system-stats.ino
