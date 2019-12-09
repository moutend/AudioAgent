package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"path/filepath"
	"strings"
	"syscall"
	"unsafe"
)

type rawCommand struct {
	Type         int16
	SoundIndex   int16
	WaitDuration float64
	TextLen      int32
	TextPtr      uintptr
	SSMLLen      int32
	SSMLPtr      uintptr
}

type command struct {
	Type  int16       `json:"type"`
	Value interface{} `json:"value"`
}

type postCommandRequest struct {
	IsAsync  bool      `json:"isAsync"`
	Commands []command `json:"commands"`
}

const (
	enumPlay = 1
	enumWait = 2
	enumText = 3
	enumSSML = 4
)

func audioHandler(w http.ResponseWriter, r *http.Request) {
	var err error

	defer func() {
		w.Header().Set("Content-Type", jsonContentType)

		data, _ := json.Marshal(struct {
			Error string `json:"error"`
		}{
			Error: err.Error(),
		})

		io.Copy(w, bytes.NewBuffer(data))

		if err != nil {
			w.WriteHeader(http.StatusBadRequest)
		}
	}()

	verb := ""

	if ps := strings.Split(r.URL.Path, "/"); len(ps) > 4 {
		verb = ps[3]
	}
	switch verb {
	case "command":
		if r.Method == "POST" {
			err = postCommand(w, r)
		}
	case "enable":
		if r.Method == "GET" {
			err = getEnable()
		}
	case "disable":
		if r.Method == "GET" {
			getEnable()
		}
	case "pause":
		if r.Method == "GET" {
			err = getPause()
		}
	case "restart":
		if r.Method == "GET" {
			getRestart()
		}
	default:
		err = fmt.Errorf("Invalid request")
	}
}

func postCommand(w http.ResponseWriter, r *http.Request) error {
	buf := &bytes.Buffer{}

	if n, err := io.Copy(buf, r.Body); err != nil || n == 0 {
		logger.Fatalf("Failed to read request body")
		return fmt.Errorf("Internal error")
	}

	var req postCommandRequest

	if err := json.Unmarshal(buf.Bytes(), &req); err != nil {
		logger.Fatal(err)
		return fmt.Errorf("Internal error")
	}

	rawCommands := make([]uintptr, len(req.Commands), len(req.Commands))

	for i, v := range req.Commands {
		c := rawCommand{
			Type: v.Type,
		}
		switch v.Type {
		case enumText:
			text := v.Value.(string)
			textU16, err := syscall.UTF16FromString(text)

			if err != nil {
				logger.Fatal(err)
				return fmt.Errorf("Internal error")
			}

			textU16Ptr, err := syscall.UTF16PtrFromString(text)

			if err != nil {
				logger.Fatal(err)
				return fmt.Errorf("Internal error")
			}

			c.TextPtr = uintptr(unsafe.Pointer(textU16Ptr))
			c.TextLen = int32(len(textU16))
		case enumSSML:
			ssml := v.Value.(string)
			ssmlU16, err := syscall.UTF16FromString(ssml)

			if err != nil {
				logger.Fatal(err)
				return fmt.Errorf("Internal error")
			}

			ssmlU16Ptr, err := syscall.UTF16PtrFromString(ssml)

			if err != nil {
				logger.Fatal(err)
				return fmt.Errorf("Internal error")
			}

			c.SSMLPtr = uintptr(unsafe.Pointer(ssmlU16Ptr))
			c.SSMLLen = int32(len(ssmlU16))
		case enumWait:
			c.WaitDuration = v.Value.(float64)
		case enumPlay:
			c.SoundIndex = int16(v.Value.(float64))
		default:
		}

		rawCommands[i] = uintptr(unsafe.Pointer(&c))
	}

	var code int32

	if len(rawCommands) > 0 {
		procFeed.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(&rawCommands[0])), uintptr(len(rawCommands)))
	}
	if code != 0 {
		logger.Printf("Failed to call Feed (code=%v)", code)
		return fmt.Errorf("")
	}

	return nil
}

func getRestart() error {
	var code int32

	procFadeIn.Call(uintptr(unsafe.Pointer(&code)))

	if code != 0 {
		logger.Printf("Failed to call FadeIn (code=%v) code=%d", code)
		return fmt.Errorf("Internal error")
	}

	return nil
}

func getPause() error {
	var code int32

	procFadeOut.Call(uintptr(unsafe.Pointer(&code)))

	if code != 0 {
		logger.Fatalf("Failed to call FadeOut (code=%v)", code)
		return fmt.Errorf("Internal error")
	}

	return nil
}

func getEnable() error {
	fullLogPath := filepath.Join(logPath, "AudioNode.ltsv")
	fullLogPathU16ptr, err := syscall.UTF16PtrFromString(fullLogPath)

	if err != nil {
		logger.Fatal(err)
		return fmt.Errorf("Internal error")
	}

	var code int32

	procStart.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(fullLogPathU16ptr)), uintptr(0))

	if code != 0 {
		logger.Printf("Failed to call Start()")
		return fmt.Errorf("Internal error")
	}

	return nil
}

func getDisable() error {
	var code int32

	procQuit.Call(uintptr(unsafe.Pointer(&code)))

	if code != 0 {
		logger.Fatalf("Failed to call Quit (code=%v)", code)
		return fmt.Errorf("Internal error")
	}

	return nil
}
