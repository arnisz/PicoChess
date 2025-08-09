package main

import (
	"bufio"
	"fmt"
	"log"
	"os"
	"strconv"
	"strings"
	"time"

	"go.bug.st/serial"
)

const (
	// Default COM port settings for Pi Pico
	DEFAULT_PORT        = "COM15"
	DEFAULT_BAUDRATE    = 115200
	DEFAULT_TIMEOUT     = 3 * time.Second // Kurz für Shredder!
	DEFAULT_CONFIG_FILE = "bridge.config"

	// UCI protocol constants
	UCI_OK    = "uciok"
	READY_OK  = "readyok"
	BEST_MOVE = "bestmove"
)

type UCIBridge struct {
	port         serial.Port
	portName     string
	baudRate     int
	timeout      time.Duration
	debugMode    bool
	logFile      *os.File
	engineReady  bool
	engineName   string
	engineAuthor string
}

func NewUCIBridge() *UCIBridge {
	bridge := &UCIBridge{
		portName:     DEFAULT_PORT,
		baudRate:     DEFAULT_BAUDRATE,
		timeout:      DEFAULT_TIMEOUT,
		debugMode:    false,
		engineReady:  false,
		engineName:   "PicoChess",
		engineAuthor: "arnisz",
	}

	// Load config file first (lower priority than command line)
	bridge.loadConfig(DEFAULT_CONFIG_FILE)

	return bridge
}

func (b *UCIBridge) loadConfig(filename string) {
	b.debugLog("Attempting to load config from: %s", filename)

	file, err := os.Open(filename)
	if err != nil {
		b.debugLog("Config file not found: %s (using defaults)", filename)
		return
	}
	defer file.Close()

	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())

		// Skip comments and empty lines
		if line == "" || strings.HasPrefix(line, "#") || strings.HasPrefix(line, ";") {
			continue
		}

		// Parse key=value pairs
		if strings.Contains(line, "=") {
			parts := strings.SplitN(line, "=", 2)
			if len(parts) != 2 {
				continue
			}

			key := strings.ToLower(strings.TrimSpace(parts[0]))
			value := strings.TrimSpace(parts[1])

			// Remove quotes if present
			if (strings.HasPrefix(value, "\"") && strings.HasSuffix(value, "\"")) ||
				(strings.HasPrefix(value, "'") && strings.HasSuffix(value, "'")) {
				value = value[1 : len(value)-1]
			}

			switch key {
			case "port", "com_port", "serial_port":
				b.portName = value
				b.debugLog("Config: Port set to %s", value)

			case "baudrate", "baud_rate", "baud":
				if baud, err := strconv.Atoi(value); err == nil && baud > 0 {
					b.baudRate = baud
					b.debugLog("Config: BaudRate set to %d", baud)
				}

			case "timeout":
				if timeout, err := strconv.Atoi(value); err == nil && timeout > 0 {
					b.timeout = time.Duration(timeout) * time.Second
					b.debugLog("Config: Timeout set to %d seconds", timeout)
				}

			case "debug":
				b.debugMode = strings.ToLower(value) == "true" || value == "1"
				b.debugLog("Config: Debug mode set to %v", b.debugMode)

			case "logfile", "log_file":
				if value != "" {
					if logFile, err := os.OpenFile(value, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0666); err == nil {
						if b.logFile != nil {
							b.logFile.Close()
						}
						b.logFile = logFile
						log.SetOutput(logFile)
						b.debugLog("Config: Log file set to %s", value)
					}
				}

			case "engine_name", "name":
				if value != "" {
					b.engineName = value
					b.debugLog("Config: Engine name set to %s", value)
				}

			case "engine_author", "author":
				if value != "" {
					b.engineAuthor = value
					b.debugLog("Config: Engine author set to %s", value)
				}
			}
		}
	}

	if err := scanner.Err(); err != nil {
		b.debugLog("Error reading config file: %v", err)
	} else {
		b.debugLog("Config loaded successfully from: %s", filename)
	}
}

func (b *UCIBridge) parseArgs() {
	args := os.Args[1:]
	for i := 0; i < len(args); i++ {
		switch args[i] {
		case "-config", "-c":
			if i+1 < len(args) {
				b.loadConfig(args[i+1])
				i++
			}
		case "-port", "-p":
			if i+1 < len(args) {
				b.portName = args[i+1]
				i++
			}
		case "-baud", "-b":
			if i+1 < len(args) {
				if baud, err := strconv.Atoi(args[i+1]); err == nil && baud > 0 {
					b.baudRate = baud
				}
				i++
			}
		case "-timeout", "-t":
			if i+1 < len(args) {
				if timeout, err := strconv.Atoi(args[i+1]); err == nil && timeout > 0 {
					b.timeout = time.Duration(timeout) * time.Second
				}
				i++
			}
		case "-debug", "-d":
			b.debugMode = true
		case "-log", "-l":
			if i+1 < len(args) {
				if logFile, err := os.OpenFile(args[i+1], os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0666); err == nil {
					if b.logFile != nil {
						b.logFile.Close()
					}
					b.logFile = logFile
					log.SetOutput(logFile)
				}
				i++
			}
		case "-create-config":
			b.createDefaultConfig(DEFAULT_CONFIG_FILE)
			fmt.Println("Default config file created: " + DEFAULT_CONFIG_FILE)
			os.Exit(0)
		case "-help", "-h":
			b.printUsage()
			os.Exit(0)
		}
	}
}

func (b *UCIBridge) createDefaultConfig(filename string) error {
	config := `# UCI Bridge Configuration for Pi Pico Chess Engine
# Lines starting with # or ; are comments

# ==============================================
# Serial Port Configuration
# ==============================================

# COM port where your Pi Pico is connected
port=COM15

# Baud rate for serial communication  
baudrate=115200

# Timeout for serial operations (in seconds)
# Keep this short for Shredder compatibility!
timeout=3

# ==============================================
# Engine Information
# ==============================================

engine_name=PicoChess
engine_author=arnisz

# ==============================================
# Debug Settings
# ==============================================

# Enable debug output (true/false)
debug=true

# Log file for debug output
logfile=bridge.log

# ==============================================
# Examples for different setups:
# ==============================================

# For slow Pi Pico:
# baudrate=9600
# timeout=10

# For production (no debug):
# debug=false
# # logfile=

# For different COM port:
# port=COM3
`

	return os.WriteFile(filename, []byte(config), 0666)
}

func (b *UCIBridge) printUsage() {
	fmt.Println("UCI Bridge for Pi Pico Chess Engine")
	fmt.Println("Usage: uci-bridge [options]")
	fmt.Println("")
	fmt.Println("Options:")
	fmt.Println("  -config, -c <file>   Config file (default: bridge.config)")
	fmt.Println("  -port, -p <port>     COM port (default: COM15)")
	fmt.Println("  -baud, -b <rate>     Baud rate (default: 115200)")
	fmt.Println("  -timeout, -t <sec>   Timeout in seconds (default: 3)")
	fmt.Println("  -debug, -d           Enable debug mode")
	fmt.Println("  -log, -l <file>      Log to file")
	fmt.Println("  -create-config       Create default config file")
	fmt.Println("  -help, -h            Show this help")
	fmt.Println("")
	fmt.Println("Config File Example (bridge.config):")
	fmt.Println("  port=COM15")
	fmt.Println("  baudrate=115200")
	fmt.Println("  debug=true")
	fmt.Println("  logfile=bridge.log")
	fmt.Println("")
	fmt.Println("Examples:")
	fmt.Println("  uci-bridge.exe                           # Use config file")
	fmt.Println("  uci-bridge.exe -port COM3 -debug        # Override config")
	fmt.Println("  uci-bridge.exe -config my.config        # Custom config file")
	fmt.Println("  uci-bridge.exe -create-config           # Create default config")
}

func (b *UCIBridge) debugLog(format string, args ...interface{}) {
	if b.debugMode {
		timestamp := time.Now().Format("15:04:05.000")
		message := fmt.Sprintf(format, args...)
		log.Printf("[DEBUG %s] %s", timestamp, message)
	}
}

func (b *UCIBridge) errorLog(format string, args ...interface{}) {
	timestamp := time.Now().Format("15:04:05.000")
	message := fmt.Sprintf(format, args...)
	log.Printf("[ERROR %s] %s", timestamp, message)
}

func (b *UCIBridge) connectToPico() error {
	b.debugLog("=== CONNECTING TO PICO ===")
	b.debugLog("Attempting to connect to %s at %d baud", b.portName, b.baudRate)

	mode := &serial.Mode{
		BaudRate: b.baudRate,
		Parity:   serial.NoParity,
		DataBits: 8,
		StopBits: serial.OneStopBit,
	}

	// Versuche Verbindung mehrmals
	var lastErr error
	for attempt := 1; attempt <= 3; attempt++ {
		b.debugLog("Connection attempt %d/3", attempt)

		port, err := serial.Open(b.portName, mode)
		if err != nil {
			lastErr = err
			b.debugLog("Attempt %d failed: %v", attempt, err)
			time.Sleep(200 * time.Millisecond)
			continue
		}

		b.port = port
		b.debugLog("Successfully connected to %s", b.portName)

		// Give Pi Pico time to initialize
		time.Sleep(100 * time.Millisecond)

		b.debugLog("=== CONNECTION ESTABLISHED ===")
		return nil
	}

	b.errorLog("Failed to connect after 3 attempts: %v", lastErr)
	return fmt.Errorf("failed to connect to %s: %v", b.portName, lastErr)
}

func (b *UCIBridge) sendToPico(command string) error {
	if b.port == nil {
		return fmt.Errorf("serial port not connected")
	}

	command = strings.TrimSpace(command)
	if command == "" {
		return nil
	}

	b.debugLog("Bridge -> Pico: %s", command)

	// Send command with newline
	_, err := b.port.Write([]byte(command + "\n"))
	if err != nil {
		return fmt.Errorf("failed to write to serial port: %v", err)
	}

	return nil
}

func (b *UCIBridge) readFromPico() (string, error) {
	if b.port == nil {
		return "", fmt.Errorf("serial port not connected")
	}

	// Set shorter read timeout for individual reads
	b.port.SetReadTimeout(100 * time.Millisecond)

	buffer := make([]byte, 1)
	var response strings.Builder
	totalTimeout := 2 * time.Second

	startTime := time.Now()
	for time.Since(startTime) < totalTimeout {
		n, err := b.port.Read(buffer)
		if err != nil {
			if strings.Contains(err.Error(), "timeout") {
				// Short timeout - continue collecting
				continue
			}
			return "", fmt.Errorf("read error: %v", err)
		}

		if n > 0 {
			char := string(buffer[0])
			response.WriteString(char)

			// Check if we have complete line
			if char == "\n" {
				break
			}

			// Reset timeout when we receive data
			startTime = time.Now()
		}
	}

	result := strings.TrimSpace(response.String())
	if result != "" {
		b.debugLog("Pico -> Bridge: %s", result)
	}

	return result, nil
}

func (b *UCIBridge) sendToGUI(message string) {
	message = strings.TrimSpace(message)
	if message == "" {
		return
	}

	b.debugLog("Bridge -> GUI: %s", message)
	fmt.Println(message)
}

func (b *UCIBridge) handleUCICommand(command string) error {
	command = strings.TrimSpace(command)
	b.debugLog("GUI -> Bridge: %s", command)

	switch {
	case command == "uci":
		// CRITICAL: Shredder expects immediate UCI response!
		b.debugLog("=== UCI INITIALIZATION START ===")

		// Send UCI command to Pico
		err := b.sendToPico(command)
		if err != nil {
			b.debugLog("Failed to send UCI to Pico: %v", err)
		} else {
			// Wait for complete UCI response from Pico
			b.debugLog("Waiting for complete UCI response from Pico...")

			// Read until we get uciok or timeout
			uciComplete := false
			deadline := time.Now().Add(3 * time.Second)

			for time.Now().Before(deadline) && !uciComplete {
				response, err := b.readFromPico()
				if err != nil {
					b.debugLog("UCI read error: %v", err)
					break
				}

				if response != "" {
					b.debugLog("UCI response from Pico: %s", response)

					// Forward response to GUI (except uciok, we handle that ourselves)
					if response != UCI_OK && !strings.HasPrefix(response, "id ") {
						b.sendToGUI(response)
					}

					// Check if UCI sequence is complete
					if response == UCI_OK {
						uciComplete = true
						b.debugLog("Received uciok from Pico - UCI sequence complete")
					}
				}

				time.Sleep(50 * time.Millisecond)
			}

			if !uciComplete {
				b.debugLog("UCI sequence incomplete or timeout - proceeding anyway")
			}
		}

		// ALWAYS send standard UCI response to GUI
		b.sendToGUI(fmt.Sprintf("id name %s", b.engineName))
		b.sendToGUI(fmt.Sprintf("id author %s", b.engineAuthor))

		// Send some basic engine options
		b.sendToGUI("option name Skill Level type spin default 10 min 1 max 20")
		b.sendToGUI("option name Move Overhead type spin default 100 min 0 max 1000")

		// CRITICAL: Must send uciok!
		b.sendToGUI(UCI_OK)
		b.engineReady = true
		b.debugLog("=== UCI INITIALIZATION COMPLETE ===")

		// IMPORTANT: Give Pico time to finish before next command
		time.Sleep(200 * time.Millisecond)

	case command == "isready":
		b.debugLog("=== ISREADY CHECK ===")

		// Give Pi Pico time to finish any previous command
		time.Sleep(100 * time.Millisecond)

		// Send isready and wait for proper response
		err := b.sendToPico(command)
		if err != nil {
			b.debugLog("Failed to send isready: %v", err)
		} else {
			// Wait specifically for "readyok" response
			deadline := time.Now().Add(2 * time.Second)

			for time.Now().Before(deadline) {
				response, err := b.readFromPico()
				if err != nil {
					b.debugLog("Isready read error: %v", err)
					break
				}

				if response != "" {
					b.debugLog("Isready response from Pico: %s", response)

					// Check if this is the readyok we're looking for
					if response == READY_OK {
						b.debugLog("Received correct readyok from Pico")
						break
					} else {
						// This might be a delayed response from previous command
						b.debugLog("Ignoring unexpected response during isready: %s", response)
					}
				}

				time.Sleep(50 * time.Millisecond)
			}
		}

		// ALWAYS send readyok to GUI
		b.sendToGUI(READY_OK)
		b.debugLog("=== ISREADY COMPLETE ===")

		// Give Pi Pico time before next command
		time.Sleep(100 * time.Millisecond)

	case command == "quit":
		b.debugLog("=== QUIT COMMAND ===")
		if b.port != nil {
			b.sendToPico(command)
			time.Sleep(100 * time.Millisecond)
		}
		return fmt.Errorf("quit command received")

	case strings.HasPrefix(command, "go "):
		b.debugLog("=== GO COMMAND: %s ===", command)

		// Send go command to Pico
		err := b.sendToPico(command)
		if err != nil {
			b.errorLog("Failed to send go command: %v", err)
			// Send fallback move
			b.sendToGUI("bestmove e2e4")
			return nil
		}

		// Calculate timeout based on command
		timeout := 30 * time.Second
		if strings.Contains(command, "movetime") {
			// Parse movetime from command
			parts := strings.Split(command, " ")
			for i, part := range parts {
				if part == "movetime" && i+1 < len(parts) {
					if ms, err := strconv.Atoi(parts[i+1]); err == nil {
						timeout = time.Duration(ms+2000) * time.Millisecond // +2s buffer
					}
				}
			}
		} else if strings.Contains(command, "wtime") {
			// Parse time control - use 10% of available time + 2s buffer
			parts := strings.Split(command, " ")
			for i, part := range parts {
				if part == "wtime" && i+1 < len(parts) {
					if wtime, err := strconv.Atoi(parts[i+1]); err == nil {
						calculatedTime := time.Duration(wtime/10) * time.Millisecond
						if calculatedTime > 1*time.Second && calculatedTime < 30*time.Second {
							timeout = calculatedTime + 2*time.Second
						}
					}
				}
			}
		}

		b.debugLog("Using timeout: %v for go command", timeout)

		// Read response(s) until we get bestmove
		bestmoveReceived := make(chan bool, 1)

		go func() {
			defer func() { bestmoveReceived <- true }()

			for {
				response, err := b.readFromPico()
				if err != nil {
					b.debugLog("Go response error: %v", err)
					return
				}

				if response != "" {
					// Send the complete line to GUI
					b.sendToGUI(response)

					// Check if this is the final bestmove
					if strings.HasPrefix(response, BEST_MOVE) {
						b.debugLog("Received final bestmove: %s", response)
						return
					}
				}

				time.Sleep(10 * time.Millisecond)
			}
		}()

		// Wait for bestmove with timeout
		select {
		case <-bestmoveReceived:
			b.debugLog("Bestmove received successfully")
		case <-time.After(timeout):
			b.debugLog("Go timeout after %v - sending fallback move", timeout)
			b.sendToGUI("bestmove e2e4")
		}

		b.debugLog("=== GO COMMAND COMPLETE ===")

	default:
		// Forward all other commands directly
		b.debugLog("=== OTHER COMMAND: %s ===", command)

		// Give Pi Pico time to finish previous command
		time.Sleep(50 * time.Millisecond)

		err := b.sendToPico(command)
		if err != nil {
			b.debugLog("Failed to send command %s: %v", command, err)
			return nil // Don't fail on unknown commands
		}

		// For position commands, don't expect immediate response
		if strings.HasPrefix(command, "position") ||
			strings.HasPrefix(command, "ucinewgame") ||
			strings.HasPrefix(command, "setoption") {
			b.debugLog("Command sent, no response expected")
			time.Sleep(50 * time.Millisecond) // Give Pi Pico time to process
			return nil
		}

		// For other commands, try to read response with short timeout
		deadline := time.Now().Add(500 * time.Millisecond)
		for time.Now().Before(deadline) {
			response, err := b.readFromPico()
			if err != nil {
				break
			}
			if response != "" {
				b.debugLog("Response to %s: %s", command, response)
				b.sendToGUI(response)
				break
			}
			time.Sleep(50 * time.Millisecond)
		}

		// Small delay before next command
		time.Sleep(50 * time.Millisecond)
	}

	return nil
}

func (b *UCIBridge) run() error {
	defer func() {
		if b.port != nil {
			b.port.Close()
		}
		if b.logFile != nil {
			b.logFile.Close()
		}
	}()

	// Try to connect to Pi Pico
	err := b.connectToPico()
	if err != nil {
		b.errorLog("Connection failed: %v", err)
		b.debugLog("WARNING: Continuing without Pi Pico connection - using fallback responses")
		// Don't return error - continue with fallback behavior
	}

	b.debugLog("UCI Bridge started successfully")
	b.debugLog("Configuration: Port=%s, Baud=%d, Timeout=%v, Debug=%v",
		b.portName, b.baudRate, b.timeout, b.debugMode)

	// Read commands from GUI (stdin)
	scanner := bufio.NewScanner(os.Stdin)

	for scanner.Scan() {
		command := scanner.Text()

		err := b.handleUCICommand(command)
		if err != nil {
			if strings.Contains(err.Error(), "quit") {
				b.debugLog("Received quit command, exiting gracefully")
				break
			}
			b.errorLog("Command handling error: %v", err)

			// For critical errors, try to reconnect
			if strings.Contains(err.Error(), "serial") && b.port != nil {
				b.debugLog("Attempting to reconnect...")
				b.port.Close()
				b.port = nil
				if err := b.connectToPico(); err != nil {
					b.debugLog("Reconnection failed: %v", err)
					// Continue without connection
				}
			}
		}
	}

	if err := scanner.Err(); err != nil {
		return fmt.Errorf("stdin reading error: %v", err)
	}

	return nil
}

func main() {
	bridge := NewUCIBridge()

	// Parse command line arguments (overrides config file)
	bridge.parseArgs()

	// Setup logging
	if bridge.logFile == nil && bridge.debugMode {
		// Default debug log to stderr when no log file specified
		log.SetOutput(os.Stderr)
	}

	bridge.debugLog("Starting UCI Bridge for Pi Pico Chess Engine")
	bridge.debugLog("Configuration: Port=%s, Baud=%d, Timeout=%v, Debug=%v",
		bridge.portName, bridge.baudRate, bridge.timeout, bridge.debugMode)

	if err := bridge.run(); err != nil {
		bridge.errorLog("Bridge error: %v", err)
		os.Exit(1)
	}
}
