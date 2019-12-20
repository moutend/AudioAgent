package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"path/filepath"
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

func postAudioCommand(w http.ResponseWriter, r *http.Request) error {
	var isForcePush int32

	if r.URL.Query().Get("force") != "" {
		isForcePush = 1
	}

	buf := &bytes.Buffer{}

	if _, err := io.Copy(buf, r.Body); err != nil {
		logger.Println(err)
		return fmt.Errorf("Requested JSON is broken")
	}

	var req postCommandRequest

	if err := json.Unmarshal(buf.Bytes(), &req); err != nil {
		logger.Println(err)
		return fmt.Errorf("Requested JSON is invalid")
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
				logger.Println(err)
				return fmt.Errorf("Internal error")
			}

			textU16Ptr, err := syscall.UTF16PtrFromString(text)

			if err != nil {
				logger.Println(err)
				return fmt.Errorf("Internal error")
			}

			c.TextPtr = uintptr(unsafe.Pointer(textU16Ptr))
			c.TextLen = int32(len(textU16))
		case enumSSML:
			ssml := v.Value.(string)
			ssmlU16, err := syscall.UTF16FromString(ssml)

			if err != nil {
				logger.Println(err)
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
		procPush.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(&rawCommands[0])), uintptr(len(rawCommands)), uintptr(isForcePush))
	}
	if code != 0 {
		logger.Printf("Failed to call Push (code=%v)", code)
		return fmt.Errorf("Internal error")
	}
	if _, err := io.WriteString(w, "{}"); err != nil {
		logger.Println(err)
		return fmt.Errorf("Internal error")
	}

	return nil
}

func getAudioRestart(w http.ResponseWriter, r *http.Request) error {
	var code int32

	procFadeIn.Call(uintptr(unsafe.Pointer(&code)))

	if code != 0 {
		logger.Printf("Failed to call FadeIn (code=%v) code=%d", code)
		return fmt.Errorf("Internal error")
	}
	if _, err := io.WriteString(w, "{}"); err != nil {
		logger.Println(err)
		return fmt.Errorf("Internal error")
	}

	return nil
}

func getAudioPause(w http.ResponseWriter, r *http.Request) error {
	var code int32

	procFadeOut.Call(uintptr(unsafe.Pointer(&code)))

	if code != 0 {
		logger.Fatalf("Failed to call FadeOut (code=%v)", code)
		return fmt.Errorf("Internal error")
	}
	if _, err := io.WriteString(w, "{}"); err != nil {
		logger.Println(err)
		return fmt.Errorf("Internal error")
	}

	return nil
}

func getAudioEnable(w http.ResponseWriter, r *http.Request) error {
	fullLogPath := filepath.Join(logPath, "AudioNode.ltsv")
	fullLogPathU16ptr, err := syscall.UTF16PtrFromString(fullLogPath)

	if err != nil {
		logger.Println(err)
		return fmt.Errorf("Internal error")
	}

	var code int32

	procSetup.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(fullLogPathU16ptr)), uintptr(0))

	if code != 0 {
		logger.Printf("Failed to call Setup()")
		return fmt.Errorf("Internal error")
	}
	if _, err := io.WriteString(w, "{}"); err != nil {
		logger.Println(err)
		return fmt.Errorf("Internal error")
	}
	return nil
}

func getAudioDisable(w http.ResponseWriter, r *http.Request) error {
	var code int32

	procTeardown.Call(uintptr(unsafe.Pointer(&code)))

	if code != 0 {
		logger.Println("Failed to call Teardown (code=%v)", code)
		return fmt.Errorf("Internal error")
	}
	if _, err := io.WriteString(w, "{}"); err != nil {
		logger.Println(err)
		return fmt.Errorf("Internal error")
	}

	return nil
}
