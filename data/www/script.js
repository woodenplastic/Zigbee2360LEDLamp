let wsJson;
let led_instances = [];

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
                    led_instances[led.index] = new LED(led.index);
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
    constructor(index, position) {
      this.index = index;
      this.position = position
      this.positionContainer = document.getElementById("sliderContainers");
      this.createElements();
      this.addEventListeners();
    }
  
    createElements() {
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
      this.slider1.min = 0;
      this.slider1.max = 1024;
      this.slider1.step = 10;
      this.slider1.value = 0;

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
      this.slider2.min = 0;
      this.slider2.max = 1024;
      this.slider2.step = 10;
      this.slider2.value = 0;

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
        element.addEventListener('change', () => {
          const data = { "led":{"index":this.index, "warmCycle": this.slider1.value, "coldCycle": this.slider2.value}  };
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
  













  

  document.addEventListener('DOMContentLoaded', function () {

    wsJson = new WebSocketJson("ws://" + window.location.hostname + "/ws");
  });

