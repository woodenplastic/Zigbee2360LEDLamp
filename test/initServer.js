const WebSocket = require("ws");

const config = {
  leds: [
    { index: 0, warmCycle: 0, coldCycle: 1013, position:"FRONT" },
    { index: 1, warmCycle: 0, coldCycle: 1013, position:"AURA" },
  ],
};

// Create a WebSocket server on port 5500
const wss = new WebSocket.Server({ port: 5500 });

wss.on("connection", (ws) => {
  console.log("Client connected");

  // Function to send messages to the client
  const sendMessages = () => {
    ws.send(JSON.stringify(config));
  };

  // Send the first message immediately
  sendMessages();

  // Send a message to the client every 30 seconds
  const intervalId = setInterval(sendMessages, 30000);

  // Listen for incoming messages and print them
  ws.on("message", (message) => {
    console.log("Raw message:", message); // Log the raw message
    try {
      const decodedMessage = JSON.parse(message);
      console.log("Received:", JSON.stringify(decodedMessage, null, 2)); // Pretty-print the received JSON

      // Echo the JSON message back to the sender
      ws.send(JSON.stringify(decodedMessage));
    } catch (error) {
      console.error("Failed to decode message as JSON:", error);
    }
  });

  ws.on("close", () => {
    console.log("Client disconnected");
    clearInterval(intervalId); // Clear the interval when the client disconnects
  });
});

console.log("Init Test WebSocket server is running on ws://localhost:5500");
