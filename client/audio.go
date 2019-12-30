package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"math"
	"net/http"
	"syscall"
	"unsafe"
)

type rawCommand struct {
	Type         int16
	SFXIndex     int16
	WaitDuration uintptr
	Text         uintptr
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
	enumSFX  = 1
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

	logger.Printf("body=%s\n", buf.Bytes())
	var req postCommandRequest

	if err := json.Unmarshal(buf.Bytes(), &req); err != nil {
		logger.Println(err)
		return fmt.Errorf("Requested JSON is invalid")
	}

	cs := make([]rawCommand, len(req.Commands), len(req.Commands))
	ps := make([]uintptr, len(req.Commands), len(req.Commands))

	for i, v := range req.Commands {
		cs[i].Type = v.Type

		switch v.Type {
		case enumSFX:
			cs[i].SFXIndex = int16(v.Value.(float64))
		case enumWait:
			cs[i].WaitDuration = uintptr(math.Float64bits(v.Value.(float64)))
		case enumText, enumSSML:
			text := v.Value.(string)
			textU16Ptr, err := syscall.UTF16PtrFromString(text)

			if err != nil {
				logger.Println(err)
				return fmt.Errorf("Internal error")
			}

			cs[i].Text = uintptr(unsafe.Pointer(textU16Ptr))
		default:
			cs[i].Type = enumWait
			continue
		}

		logger.Printf("cmds[%d]=%x\n", i, uintptr(unsafe.Pointer(&cs[i])))
		ps[i] = uintptr(unsafe.Pointer(&cs[i]))
	}

	var code int32

	if len(ps) > 0 {
		procPush.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(&ps[0])), uintptr(len(ps)), uintptr(isForcePush))
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
	var code int32

	procSetup.Call(uintptr(unsafe.Pointer(&code)), uintptr(0))

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
