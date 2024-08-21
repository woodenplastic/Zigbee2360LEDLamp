let wsJson;
let wifiScanner;
let led_instances = [];
let networkModal

class WebSocketJson {
    constructor(url, maxRetries = 10) {
      this.url = url;
      this.maxRetries = maxRetries;
      this.reconnectInterval = 1000; // Initial reconnect interval in milliseconds
      this.maxReconnectInterval = 30000; // Maximum reconnect interval
      this.reconnectAttempts = 0;
      this.connect();
    }
  
    connect() {
      this.ws = new WebSocket(this.url);
  
      this.ws.onopen = () => {
        console.log("WebSocket connection established");
        this.reconnectAttempts = 0; // Reset attempts on successful connection
  
      };
  
      this.ws.onmessage = (event) => {
        console.log("Received data: " + event.data);
        try {
          const data = JSON.parse(event.data);

          if (data.hasOwnProperty("leds") && Array.isArray(data.leds)) {
                data.leds.forEach(led => {
                    // Create an instance if it doesn't exist
                    if (!led_instances[led.index]) {
                    led_instances[led.index] = new LED(led);
                    }

                    led_instances[led.index].set(led);
                });
                
            }
  
        } catch (e) {
          console.error("Error processing the message:", e);
        }
      };
      
  
      this.ws.onclose = (event) => {
  
        console.log(`WebSocket connection closed: ${event.reason}`);
        this.reconnect();
      };
  
      this.ws.onerror = (error) => {
        console.log(`WebSocket error: ${error.message}`);
        this.ws.close();
      };
    }
  
    reconnect() {
      if (this.reconnectAttempts >= this.maxRetries) {
        console.log(
          "Maximum reconnect attempts reached. No longer attempting to reconnect."
        );
        return;
      }
  
      this.reconnectAttempts++;
      const reconnectInterval = Math.min(
        this.reconnectInterval * 2 ** this.reconnectAttempts,
        this.maxReconnectInterval
      );
      console.log(
        `Attempting to reconnect in ${reconnectInterval / 1000} seconds`
      );
  
    }
  
    send(message) {
      if (this.ws.readyState === WebSocket.OPEN) {
        this.ws.send(message);
      } else {
        console.log("WebSocket is not open. Message not sent.");
      }
    }
  
    close() {
      this.ws.close();
    }
  }




  class LED {
    constructor(data, position) {
      this.index = data.index;
      this.position = position
      this.positionContainer = document.getElementById("sliderContainers");
      this.createElements(data);
      this.addEventListeners();
    }
  
    createElements(data) {
      this.slidercontainer = document.createElement('div');
      this.slidercontainer.className = 'slider-container';
      
      this.sliderlabel = document.createElement('div');
      this.sliderlabel.className = 'slider-label';
      //this.sliderlabel.innerHTML = `${this.position}`;
      this.sliderlabel.innerHTML = "FRONT";

      this.slidercontainer1 = document.createElement('div');
      this.slidercontainer1.className = 'slider slider1';

      this.slider1 = document.createElement('input');
      this.slider1.type = 'range';
      this.slider1.className = 'slider';
      this.slider1.id = `slider1_${this.index}`;
      this.slider1.min = 100;
      this.slider1.max = 1024;
      this.slider1.step = 10;
      this.slider1.value = data.warmCycle;

      this.slider1text = document.createElement('p');
      this.slider1text.className = 'state';
      this.slider1text.id = `slider1text_${this.index}`;
      this.slider1text.innerHTML = `Brightness: <span id="sliderValue_${this.index}"></span> &percnt`;


      this.slidercontainer2 = document.createElement('div');
      this.slidercontainer2.className = 'slider slider2';

      this.slider2 = document.createElement('input');
      this.slider2.type = 'range';
      this.slider2.className = 'slider';
      this.slider2.id = `slider2_${this.index}`;
      this.slider2.min = 150;
      this.slider2.max = 1024;
      this.slider2.step = 10;
      this.slider2.value = data.coldCycle;

      this.slider2text = document.createElement('p');
      this.slider2text.className = 'state';
      this.slider2text.id = `slider2text_${this.index}`;
      this.slider2text.innerHTML = `Brightness: <span id="slider2Value_${this.index}"></span> &percnt`;


      this.slidercontainer.appendChild(this.sliderlabel);

      this.slidercontainer.appendChild(this.slidercontainer1);
      this.slidercontainer1.appendChild(this.slider1);
      this.slidercontainer1.appendChild(this.slider1text);

      this.slidercontainer.appendChild(this.slidercontainer2);
      this.slidercontainer2.appendChild(this.slider2);
      this.slidercontainer2.appendChild(this.slider2text);

      this.positionContainer.appendChild(this.slidercontainer);
    }
  
    addEventListeners() {
  
      this.slidercontainer.querySelectorAll('input').forEach(element => {
        element.addEventListener('input', () => {
          const data = { "leds":{"index":this.index, "warmCycle": this.slider1.value, "coldCycle": this.slider2.value}  };
          wsJson.send(JSON.stringify(data));

        });
      });

    }

    set(data) {
         this.slider1.value = data.warmCycle
         this.slider2.value = data.coldCycle
         this.sliderlabel.innerHTML = data.position
    }
  
  }
  








  class WiFiScanner {
    constructor(
      scanButtonId,
      spinnerId,
      wifiListId,
      ssidInputId,
      passwordInputId
    ) {
      this.scanButton = document.getElementById(scanButtonId);
      this.spinner = document.getElementById(spinnerId);
      this.wifiList = document.getElementById(wifiListId);
      this.ssidInput = document.getElementById(ssidInputId);
      this.passwordInput = document.getElementById(passwordInputId);
    }
  
    // Start the WiFi scan process
    async startScan() {
      this.toggleButtonSpinner(true);
      await this.performWifiScan();
      this.toggleButtonSpinner(false);
    }
  
    async performWifiScan() {
      try {
        const response = await fetch("/doScanWiFi");
        if (!response.ok) {
          this.handleError("Error");
          throw new Error("Network response was not ok");
        }
        const data = await response.json();
        this.updateWifiList(data);
      } catch (error) {
        this.handleError("Error");
        console.error("Error:", error);
      } finally {
        this.toggleButtonSpinner(false);
      }
    }
  
    handleError(errorMessage) {
      if (this.scanButton) {
        this.scanButton.innerHTML = errorMessage;
        this.scanButton.style.background = "red";
        this.scanButton.style.color = "white";
  
        setTimeout(() => {
          if (this.scanButton) {
            this.scanButton.innerHTML = "Scan WiFi";
            this.scanButton.style.background = "";
            this.scanButton.style.color = "";
          }
        }, 5000);
      }
    }
  
    // Toggle the visibility of the button and spinner
    toggleButtonSpinner(show) {
      if (this.scanButton) {
        this.scanButton.innerHTML = show ? "" : "Scan WiFi";
      }
      if (this.spinner) {
        this.spinner.style.display = show ? "inline-block" : "none";
      }
    }
  
    // Update the WiFi list in the UI
    updateWifiList(networks) {
      this.wifiList.innerHTML = ""; // Clear existing list
  
      const list = document.createElement("ul");
      list.style.listStyle = "none";
      list.style.padding = "0";
  
      networks.forEach((network) => {
        const listItem = document.createElement("li");
  
        // Apply styles based on the current theme
        this.applyThemeStyles(listItem);
  
        listItem.style.margin = "5px 0";
        listItem.style.padding = "10px";
        listItem.style.cursor = "pointer";
  
        listItem.onclick = () => {
          this.ssidInput.value = network.ssid;
          this.passwordInput.focus();
        };
  
        const ssidElement = document.createElement("div");
        ssidElement.textContent = `SSID: ${network.ssid}`;
        listItem.appendChild(ssidElement);
  
        const signalElement = document.createElement("div");
        signalElement.textContent = `Signal: ${this.getSignalStrength(
          network.rssi
        )}`;
        listItem.appendChild(signalElement);
  
        const encryptionElement = document.createElement("div");
        encryptionElement.textContent = `Encryption: ${this.getEncryptionType(
          network.encryption
        )}`;
        listItem.appendChild(encryptionElement);
  
        list.appendChild(listItem);
      });
  
      this.wifiList.appendChild(list);
    }
  
    // Apply theme-specific styles to the list item
    applyThemeStyles(listItem) {
      if (document.body.classList.contains("dark-mode")) {
        // Styles for dark mode
        listItem.style.border = "1px solid #3498db"; // Blue border
        listItem.style.backgroundColor = "#000"; // Dark background
        listItem.style.color = "#fff"; // White text
      } else {
        // Styles for light mode
        listItem.style.border = "1px solid #ccc"; // Light grey border
        listItem.style.backgroundColor = "#f9f9f9"; // Light background
        listItem.style.color = "#333"; // Dark text
      }
    }
  
    // Get encryption type description
    getEncryptionType(encryptionType) {
      switch (encryptionType) {
        case 0:
          return "Open";
        case 1:
          return "WEP";
        case 2:
          return "WPA PSK";
        case 3:
          return "WPA2 PSK";
        case 4:
          return "WPA/WPA2 PSK";
        case 5:
          return "WPA2 Enterprise";
        case 6:
          return "Max";
        default:
          return "Unknown";
      }
    }
  
    // Get signal strength description
    getSignalStrength(rssi) {
      if (rssi >= -50) return "Excellent";
  
      if (rssi >= -60) return "Very Good";
  
      if (rssi >= -70) return "Good";
  
      if (rssi >= -80) return "Moderate";
  
      return "Weak";
    }
  }
  


  class ModalBase {
    constructor(modalId, index) {
      this.modal = document.getElementById(modalId);
      this.openBtn = document.getElementById(modalId.replace("_modal", "_bmodal"));
      this.background = document.getElementById("modal-background");
      this.closeBtn = this.modal.querySelector(".modal-close");
      this.plusButton = this.modal.querySelector(".plusButton");
      this.content = this.modal.querySelector(".modal-content");
      this.index = index;
      this.componentCounter = [];
  
      this.init();
    }
  
    init() {
      if (this.closeBtn) {
        this.closeBtn.addEventListener("click", () => this.closeModal());
      }
  
  
      if (this.openBtn) {
        this.openBtn.addEventListener("click", () => this.openModal());
      }
  
      if (this.background) {
        this.background.addEventListener("click", () => this.closeModal());
      }
    }
  
    openModal() {
      if (this.modal && this.background) {
        this.modal.style.display = "block";
        this.background.style.display = "block";
        document.body.style.overflow = "hidden";
      }
    }
  
    closeModal() {
      if (this.modal && this.background) {
        this.modal.style.display = "none";
        this.background.style.display = "none";
        document.body.style.overflow = "auto";
      }
    }
  
  }




  
async function handleSaveWifi() {
    const button = document.getElementById("saveWifiButton");
    const spinner = document.getElementById("saveWifiSpinner");
  
    toggleButtonSpinner(button, spinner, true);
  
    const formData = {
      ssid: document.getElementById("ssid").value,
      password: document.getElementById("password").value,
    };
  
    await handleButtonClick(button, "/credWifi", formData, spinner);
  }
  
  async function handleSaveWifiStatic() {
    const button = document.getElementById("saveSWifiButton");
    const spinner = document.getElementById("saveSWifiSpinner");
  
    toggleButtonSpinner(button, spinner, true);
  
    const formData = {
      useStaticWiFi: document.getElementById("useStaticWiFi").checked,
      Swifi_ip: document.getElementById("Swifi_ip").value,
      Swifi_subnet: document.getElementById("Swifi_subnet").value,
      Swifi_gw: document.getElementById("Swifi_gw").value,
      Swifi_dns: document.getElementById("Swifi_dns").value,
    };
  
    await handleButtonClick(button, "/credSWifi", formData, spinner);
  }
  
  async function handleButtonClick(buttonElement, endpoint, formData, spinner) {
    console.log("Button clicked. Starting request...");
    try {
      console.log("Sending request to:", endpoint);
      const response = await fetch(endpoint, {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify(formData),
      });
  
      console.log("Response received:", response);
      if (!response.ok) {
        throw new Error("Network response was not ok");
      }
  
      if (endpoint === "/credWifi" || endpoint === "/credSWifi") {
        alert(
          "Connect to selected WiFi and connect again with almaloox.local"
        );
        const redirectUrl =
          "http://almaloox.local.local";
        console.log("Redirecting to:", redirectUrl);
        window.location.href = redirectUrl;
      } else {
        console.log("Handling other responses...");
        // Handle other responses as needed
      }
    } catch (error) {
      console.error("Error:", error);
    } finally {
      console.log("Entering finally block");
      toggleButtonSpinner(buttonElement, spinner, false);
    }
  }
  
  function toggleButtonSpinner(button, spinner, show) {
    if (button) {
      button.style.display = show ? "none" : "inline-block";
    }
    if (spinner) {
      spinner.style.display = show ? "inline-block" : "none";
    }
  }
  
  function handlePasswordVisibilityToggle() {
    const password = document.getElementById("password");
    password.type = this.checked ? "text" : "password";
  }




  

  document.addEventListener('DOMContentLoaded', function () {

    

    wifiScanner = new WiFiScanner(
        "scanWiFi",
        "scanWiFiSpinner",
        "wifi-list",
        "ssid",
        "password"
      );

      document.getElementById("toggle-password").addEventListener("change", handlePasswordVisibilityToggle);

      networkModal = new ModalBase("wifi_modal");
      
    wsJson = new WebSocketJson("ws://" + window.location.hostname + "/ws");
    });

