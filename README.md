Smart Waste Management System using IoT and Machine Learning

📌 Project Overview

This project presents a Smart Waste Level Detection and Prediction system using IoT and Machine Learning. The system monitors garbage levels in real time using an ultrasonic sensor connected to an ESP8266 microcontroller and predicts future waste levels using machine learning models trained on real IoT data.

The solution integrates embedded systems with predictive analytics to make waste management smarter and more efficient.


 Features

- Real-time garbage level monitoring
- WiFi-based live web dashboard (ESP8266)
- LCD display showing level and distance
- Buzzer alert when bin exceeds 80% capacity
- Hourly dataset collection for ML training
- Machine Learning-based prediction
- Model comparison using evaluation metrics



 🛠️ Technologies Used

 🔹 IoT Layer:
- ESP8266 (NodeMCU)
- Ultrasonic Sensor (HC-SR04)
- LCD (I2C)
- Buzzer
- C++ (Arduino Framework)
- HTML, CSS, JavaScript (Embedded Web Server)

 🔹 Machine Learning Layer:
- Python
- Google Colab
- Pandas
- NumPy
- Scikit-learn
- Matplotlib

System Architecture:
1. Ultrasonic sensor measures garbage distance.
2. ESP8266 converts distance into garbage level percentage.
3. Data is displayed on:
   - LCD screen
   - Local web dashboard via device IP
4. Hourly readings are recorded for dataset preparation.
5. Dataset is used to train ML models.
6. Best model selected based on R² score and MSE.

📊 Machine Learning Implementation:

 Dataset Features
- Hour
- Distance (cm)

 Target
- Garbage Level (%)

 Models Used
- Linear Regression
- Random Forest Regressor

Model Performance:
 Linear Regression
- R² Score: 0.937
- Mean Squared Error: 50.85

 Random Forest
- R² Score: 0.987
- Mean Squared Error: 10.58

Random Forest achieved better performance due to its ability to capture non-linear patterns in garbage generation.

📁 Repository Structure
- `SmartWasteIoT.ino` → ESP8266 IoT firmware
- `Training.ipynb` → Machine Learning training and evaluation
- `Dataset Swms.csv` → Real IoT dataset used for ML training
- `System Architecture.png` → Project architecture diagram
- `README.md` → Project documentation

  Author
  Srija Kandimalla  
