let wsJson;

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
        dataReadyTimeout = setTimeout(function () {
          document.dispatchEvent(new Event('dataLoaded'));
        }, 1); // Delay of 1000 milliseconds (1 second)
  
      };
  
      this.ws.onmessage = (event) => {
        console.log("Received data: " + event.data);
        try {
          const data = JSON.parse(event.data);

          if (data.hasOwnProperty("leds") && Array.isArray(data.leds)) {
            data.leds.forEach(led => {
                // Create an instance if it doesn't exist
                if (!led_instances[led.id]) {
                  led_instances[led.id] = new LED(led.id);
                }
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
  
      setTimeout(() => {
        console.log("Reconnecting...");
        this.connect();
      }, reconnectInterval);
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
    constructor(index) {
      this.index = index;
      this.positionContainer = document.getElementById("sliderContainers");
      this.createElements();
      this.addEventListeners();
    }
  
    createElements() {
      this.slidercontainer = document.createElement('div');
      this.slidercontainer.className = 'slider-container';
      
      this.sliderlabel = document.createElement('div');
      this.sliderlabel.className = 'slider-label';
      this.sliderlabel.innerHTML = "FRONT";

      this.slidercontainer1 = document.createElement('div');
      this.slidercontainer1.className = 'slider slider1';
   
      this.slider1 = document.createElement('input');
      this.slider1.type = 'range';
      this.slider1.className = 'slider';
      this.slider1.id = `slider1_${this.index}`;
      this.slider1.min = 0;
      this.slider1.max = 1024;
      this.slider1.step = 10;
      this.slider1.value = 0;

      this.slider1text = document.createElement('p');
      this.slider1text.className = 'state';
      this.slider1text.id = `slider1text_${this.index}`;
      this.slider1text.innerHTML = `Brightness: <span id="sliderValue_${this.index}"></span> &percnt`;


      this.slider2 = document.createElement('input');
      this.slider2.type = 'range';
      //this.slider2.className = 'slider';
      this.slider2.id = `slider2_${this.index}`;
      this.slider2.min = 0;
      this.slider2.max = 1024;
      this.slider2.step = 10;
      this.slider2.value = 0;

      this.slider2text = document.createElement('p');
      this.slider2text.className = 'state';
      this.slider2text.id = `slider2text_${this.index}`;
      this.slider2text.innerHTML = `Brightness: <span id="slider2Value_${this.index}"></span> &percnt`;

      this.slidercontainer.appendChild(this.slider1);
      this.slidercontainer.appendChild(this.slider1text);
      this.slidercontainer.appendChild(this.slider2);
      this.slidercontainer.appendChild(this.slider2text);
  
      this.positionContainer.appendChild(this.slidercontainer);
    }
  
    addEventListeners() {
  
      this.slidercontainer.querySelectorAll('input').forEach(element => {
        element.addEventListener('change', () => {
          const data = { id: this.id, [element.id]: element.value };
          wsJson.send(JSON.stringify(data));
        });
      });

    }
  
  }
  













  

  document.addEventListener('DOMContentLoaded', function () {

    wsJson = new WebSocketJson("ws://" + window.location.hostname + "/ws");
  });

