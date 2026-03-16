package main

import (
	"bufio"
	"fmt"
	"io"
	"log"
	"math"
	"strconv"
	"strings"
	"time"

	"github.com/andybrewer/mack"
	"go.bug.st/serial"
	"go.bug.st/serial/enumerator"
)

const (
	baudRate  = 115200
	syncEvery = 10 * time.Second
	maxAbbrev = 24
)

func main() {
	for {
		port, err := findPort()
		if err != nil {
			log.Printf("No serial device found: %v", err)
			time.Sleep(5 * time.Second)
			continue
		}
		log.Printf("Connected to %s", port)
		if err := monitor(port); err != nil {
			log.Printf("Lost connection: %v", err)
		}
	}
}

// Arduino Nano ESP32 USB identifiers.
const (
	nanoESP32VID = "2341"
	nanoESP32PID = "0070"
)

// findPort discovers the Arduino Nano ESP32 serial port by USB VID/PID.
func findPort() (string, error) {
	ports, err := enumerator.GetDetailedPortsList()
	if err != nil {
		return "", err
	}
	for _, p := range ports {
		if p.IsUSB && p.VID == nanoESP32VID && p.PID == nanoESP32PID {
			return p.Name, nil
		}
	}
	return "", fmt.Errorf("Nano ESP32 not found (VID=%s PID=%s)", nanoESP32VID, nanoESP32PID)
}

func monitor(portName string) error {
	port, err := serial.Open(portName, &serial.Mode{BaudRate: baudRate})
	if err != nil {
		return err
	}
	defer port.Close()

	w := &serialWriter{port: port}

	// Read serial lines in a goroutine so we can also run a sync ticker.
	lines := make(chan string)
	readErr := make(chan error, 1)
	go func() {
		scanner := bufio.NewScanner(port)
		for scanner.Scan() {
			line := strings.TrimSpace(scanner.Text())
			if line != "" {
				lines <- line
			}
		}
		readErr <- scanner.Err()
	}()

	lastTrack := sendAll(w)
	playing := isPlaying()
	sendState(w, playing)

	syncTicker := time.NewTicker(syncEvery)
	defer syncTicker.Stop()

	for {
		select {
		case line := <-lines:
			cmd := line
			var arg string
			if len(line) > 3 {
				cmd = line[:2]
				arg = line[3:]
			} else if len(line) >= 2 {
				cmd = line[:2]
			}

			switch cmd {
			case "PP":
				tellMusic("playpause")
				playing = sendState(w, isPlaying())
				lastTrack = sendAll(w)

			case "LL":
				playlists, err := tellMusic("get name of every playlist")
				if err == nil {
					w.Send("OP " + strings.ReplaceAll(playlists, ", ", "|"))
				}

			case "FA":
				log.Println("Favourite")
				tellMusic("set favorited of current track to true")

			case "SK":
				tellMusic("play next track")
				lastTrack = sendAll(w)
				playing = sendState(w, true)

			case "PL":
				log.Println("Playing playlist", arg)
				tellMusic(
					"set shuffle enabled to false",
					fmt.Sprintf("set thePlaylist to playlist %q", arg),
					"play thePlaylist",
				)
				playing = sendState(w, true)
				time.Sleep(time.Second)
				lastTrack = sendAll(w)

			case "SH":
				log.Println("Shuffle playlist", arg)
				tellMusic(
					"set shuffle enabled to true",
					"set shuffle mode to albums",
					fmt.Sprintf("set thePlaylist to playlist %q", arg),
					"play thePlaylist",
				)

			case "JU":
				log.Println("Jump", arg)
				tellMusic(fmt.Sprintf("set player position to (player position %s)", arg))
				sendTrackPos(w)

			case "SY":
				log.Println("Syncing")
				lastTrack = sendAll(w)
				playing = sendState(w, isPlaying())

			case "!!":
				log.Println("Arduino:", arg)

			default:
				log.Printf("Unknown command: %s", cmd)
			}

		case <-syncTicker.C:
			lastTrack, playing = periodicSync(w, lastTrack, playing)

		case err := <-readErr:
			return err
		}
	}
}

// periodicSync checks for track/state changes without being triggered by the Arduino.
func periodicSync(w *serialWriter, lastTrack string, wasPlaying bool) (string, bool) {
	info, err := getTrackInfo()
	if err != nil {
		return lastTrack, wasPlaying
	}

	if info.Name != lastTrack {
		sendTrackInfo(w, info)
		playing := info.State == "playing"
		sendState(w, playing)
		return info.Name, playing
	}

	nowPlaying := info.State == "playing"
	if nowPlaying != wasPlaying {
		sendState(w, nowPlaying)
	}
	if nowPlaying {
		w.Send(fmt.Sprintf("TP %d", info.Position))
	}
	return lastTrack, nowPlaying
}

// trackInfo holds all metadata fetched in a single AppleScript call.
type trackInfo struct {
	Artist   string
	Album    string
	Name     string
	Duration int // tenths of a second
	Position int // tenths of a second
	State    string
}

// getTrackInfo fetches all track metadata and player state in one AppleScript invocation.
func getTrackInfo() (trackInfo, error) {
	// Use a tab delimiter to avoid ambiguity with commas in metadata.
	result, err := tellMusic(
		`set d to "\t"`,
		`set t to current track`,
		`set o to (artist of t) & d & (album of t) & d & (name of t) & d & (duration of t as string) & d & (player position as string) & d & (player state as string)`,
		`return o`,
	)
	if err != nil {
		return trackInfo{}, err
	}

	parts := strings.Split(result, "\t")
	if len(parts) < 6 {
		return trackInfo{}, fmt.Errorf("unexpected track info format: %q", result)
	}

	return trackInfo{
		Artist:   parts[0],
		Album:    parts[1],
		Name:     parts[2],
		Duration: fixedCast(parts[3]),
		Position: fixedCast(parts[4]),
		State:    parts[5],
	}, nil
}

// sendAll fetches all track info in one call and sends it to the Arduino.
// Returns the track name for change detection.
func sendAll(w *serialWriter) string {
	info, err := getTrackInfo()
	if err != nil {
		log.Printf("Failed to get track info: %v", err)
		return ""
	}
	sendTrackInfo(w, info)
	return info.Name
}

func sendTrackInfo(w *serialWriter, info trackInfo) {
	fmt.Println()
	fmt.Println(info.Artist)
	fmt.Println(abbreviate(info.Album))
	fmt.Println(abbreviate(info.Name))

	w.Send("BA " + info.Artist)
	w.Send("AL " + abbreviate(info.Album))
	w.Send("TR " + abbreviate(info.Name))
	w.Send(fmt.Sprintf("TL %d", info.Duration))
	w.Send(fmt.Sprintf("TP %d", info.Position))
}

func sendTrackPos(w *serialWriter) {
	result, err := tellMusic("get {player position} & {duration} of current track")
	if err != nil {
		return
	}
	parts := strings.Split(result, ", ")
	if len(parts) >= 1 {
		w.Send(fmt.Sprintf("TP %d", fixedCast(parts[0])))
	}
}

func isPlaying() bool {
	state, err := tellMusic("player state")
	if err != nil {
		return false
	}
	return state == "playing"
}

func sendState(w *serialWriter, playing bool) bool {
	if playing {
		w.Send("PL")
	} else {
		w.Send("PA")
	}
	return playing
}

// tellMusic sends one or more commands to the Music app via AppleScript.
func tellMusic(commands ...string) (string, error) {
	return mack.Tell("Music", commands...)
}

// fixedCast converts a numeric string to tenths-of-a-second integer.
func fixedCast(s string) int {
	f, err := strconv.ParseFloat(strings.TrimSpace(s), 64)
	if err != nil {
		return 0
	}
	return int(math.Round(f * 10))
}

// abbreviate shortens a string for the small TFT display.
func abbreviate(s string) string {
	if len(s) <= maxAbbrev {
		return s
	}
	for _, sep := range []string{" - ", "(", " / "} {
		if i := strings.Index(s, sep); i >= 0 {
			return strings.TrimSpace(s[:i])
		}
	}
	if len(s) > maxAbbrev*2+3 {
		return s[:maxAbbrev*2] + "…"
	}
	return s
}

// serialWriter wraps a serial port with line-oriented writes.
type serialWriter struct {
	port io.ReadWriteCloser
}

func (w *serialWriter) Send(msg string) {
	_, err := w.port.Write([]byte(msg + "\n"))
	if err != nil {
		log.Printf("Serial write error: %v", err)
	}
}
