from flask import Flask
from flask import request
import sqlite3
from datetime import datetime
from flask import jsonify
from flask import render_template_string
import os

app = Flask(__name__)

HTML_TEMPLATE = """
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Light Sensor Data</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body>
    <h1>Light Sensor Dashboard</h1>
    <div id="stats">
        <p><strong>Min Light Intensity:</strong> <span id="minValue"></span></p>
        <p><strong>Max Light Intensity:</strong> <span id="maxValue"></span></p>
        <p><strong>Average Light Intensity:</strong> <span id="avgValue"></span></p>
        <p><strong>Energy Usage (Wh):</strong> <span id="energyUsage"></span></p>
    </div>
    <canvas id="lightChart" width="800" height="400"></canvas>
    <script>
        fetch('/light-sensor/data')
            .then(response => response.json())
            .then(data => {
                const timestamps = data.map(d => d.timestamp);
                const lightValues = data.map(d => d.light_value);

                // Calculate running average
                let runningAvg = [];
                lightValues.reduce((sum, val, index) => {
                    runningAvg[index] = (sum + val) / (index + 1);
                    return sum + val;
                }, 0);

                // Calculate min, max, and average values
                const minValue = Math.min(...lightValues);
                const maxValue = Math.max(...lightValues);
                const avgValue = lightValues.reduce((a, b) => a + b, 0) / lightValues.length;
                
                const totalEnergy = lightValues.reduce((sum, val) => sum + val * (10 / 3600), 0);
                
                // Display stats
                document.getElementById('minValue').textContent = minValue.toFixed(2);
                document.getElementById('maxValue').textContent = maxValue.toFixed(2);
                document.getElementById('avgValue').textContent = avgValue.toFixed(2);
                document.getElementById('energyUsage').textContent = totalEnergy.toFixed(2);

                // Create the chart
                const ctx = document.getElementById('lightChart').getContext('2d');
                new Chart(ctx, {
                    type: 'line',
                    data: {
                        labels: timestamps,
                        datasets: [
                            {
                                label: 'Light Intensity',
                                data: lightValues,
                                borderColor: 'rgba(75, 192, 192, 1)',
                                borderWidth: 2,
                                fill: false
                            },
                            {
                                label: 'Running Average',
                                data: runningAvg,
                                borderColor: 'rgba(255, 99, 132, 1)',
                                borderWidth: 2,
                                borderDash: [5, 5],
                                fill: false
                            }
                        ]
                    },
                    options: {
                        responsive: true,
                        scales: {
                            x: { title: { display: true, text: 'Time' } },
                            y: { title: { display: true, text: 'Light Intensity' } }
                        }
                    }
                });
            });
    </script>
</body>
</html>
"""

DB_NAME = 'light_sensor.db'

def init_db():
    conn = sqlite3.connect(DB_NAME)
    cursor = conn.cursor()
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS light_readings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            light_value REAL NOT NULL,
            timestamp TEXT NOT NULL
        )
    ''')
    conn.commit()
    conn.close()

@app.route('/light-sensor', methods=['POST'])
def receive_data():
    try:
        if not os.path.exists(DB_NAME):
            init_db()

        data = request.json
        light_value = data.get('light_value')
        if light_value is None:
            return jsonify({"error": "Missing 'light_value' in request"}), 400

        timestamp = datetime.utcnow().strftime('%m/%d %H:%M')

        conn = sqlite3.connect(DB_NAME)
        cursor = conn.cursor()
        cursor.execute('INSERT INTO light_readings (light_value, timestamp) VALUES (?, ?)',
                       (light_value, timestamp))
        conn.commit()
        conn.close()

        return jsonify({"status": "success"}), 201
    except sqlite3.OperationalError as db_error:
        init_db()
        return jsonify({"error": "Database error, reinitializing: " + str(db_error)}), 500
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/light-sensor/data', methods=['GET'])
def fetch_data():
    conn = sqlite3.connect(DB_NAME)
    cursor = conn.cursor()
    cursor.execute('SELECT light_value, timestamp FROM light_readings ORDER BY timestamp ASC')
    rows = cursor.fetchall()
    conn.close()

    data = [{"light_value": row[0], "timestamp": row[1]} for row in rows]
    return jsonify(data)

@app.route('/')
def index():
    return render_template_string(HTML_TEMPLATE)

if __name__ == '__main__':
    init_db()
    app.run(host='0.0.0.0', port=5000, debug=True)
