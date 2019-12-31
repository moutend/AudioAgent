package api

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"math"
	"net/http"
	"syscall"
	"unsafe"

	"github.com/moutend/AudioNode/pkg/dll"
	"github.com/moutend/AudioNode/pkg/types"
)

type command struct {
	Type  int16       `json:"type"`
	Value interface{} `json:"value"`
}

type postCommandRequest struct {
	Commands []command `json:"commands"`
}

func PostAudioCommand(w http.ResponseWriter, r *http.Request) error {
	var isForcePush int32

	if r.URL.Query().Get("force") != "" {
		isForcePush = 1
	}

	buf := &bytes.Buffer{}

	if _, err := io.Copy(buf, r.Body); err != nil {
		log.Println(err)
		return fmt.Errorf("Requested JSON is broken")
	}

	log.Printf("body=%s\n", buf.Bytes())
	var req postCommandRequest

	if err := json.Unmarshal(buf.Bytes(), &req); err != nil {
		log.Println(err)
		return fmt.Errorf("Requested JSON is invalid")
	}

	cs := make([]types.Command, len(req.Commands), len(req.Commands))
	ps := make([]uintptr, len(req.Commands), len(req.Commands))

	for i, v := range req.Commands {
		cs[i].Type = v.Type

		switch v.Type {
		case types.EnumSFX:
			cs[i].SFXIndex = int16(v.Value.(float64))
		case types.EnumWait:
			cs[i].WaitDuration = uintptr(math.Float64bits(v.Value.(float64)))
		case types.EnumText, types.EnumSSML:
			text := v.Value.(string)
			textU16Ptr, err := syscall.UTF16PtrFromString(text)

			if err != nil {
				log.Println(err)
				return fmt.Errorf("Internal error")
			}

			cs[i].Text = uintptr(unsafe.Pointer(textU16Ptr))
		default:
			cs[i].Type = types.EnumWait
			continue
		}

		ps[i] = uintptr(unsafe.Pointer(&cs[i]))
	}

	var code int32

	if len(ps) > 0 {
		dll.ProcPush.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(&ps[0])), uintptr(len(ps)), uintptr(isForcePush))
	}
	if code != 0 {
		log.Printf("Failed to call Push (code=%v)", code)
		return fmt.Errorf("Internal error")
	}
	if _, err := io.WriteString(w, "{}"); err != nil {
		log.Println(err)
		return fmt.Errorf("Internal error")
	}

	return nil
}

func GetAudioRestart(w http.ResponseWriter, r *http.Request) error {
	var code int32

	dll.ProcFadeIn.Call(uintptr(unsafe.Pointer(&code)))

	if code != 0 {
		log.Printf("Failed to call FadeIn (code=%v) code=%d", code)
		return fmt.Errorf("Internal error")
	}
	if _, err := io.WriteString(w, "{}"); err != nil {
		log.Println(err)
		return fmt.Errorf("Internal error")
	}

	return nil
}

func GetAudioPause(w http.ResponseWriter, r *http.Request) error {
	var code int32

	dll.ProcFadeOut.Call(uintptr(unsafe.Pointer(&code)))

	if code != 0 {
		log.Fatalf("Failed to call FadeOut (code=%v)", code)
		return fmt.Errorf("Internal error")
	}
	if _, err := io.WriteString(w, "{}"); err != nil {
		log.Println(err)
		return fmt.Errorf("Internal error")
	}

	return nil
}

func GetAudioEnable(w http.ResponseWriter, r *http.Request) error {
	var code int32

	dll.ProcSetup.Call(uintptr(unsafe.Pointer(&code)), uintptr(0))

	if code != 0 {
		log.Printf("Failed to call Setup()")
		return fmt.Errorf("Internal error")
	}
	if _, err := io.WriteString(w, "{}"); err != nil {
		log.Println(err)
		return fmt.Errorf("Internal error")
	}
	return nil
}

func GetAudioDisable(w http.ResponseWriter, r *http.Request) error {
	var code int32

	dll.ProcTeardown.Call(uintptr(unsafe.Pointer(&code)))

	if code != 0 {
		log.Println("Failed to call Teardown (code=%v)", code)
		return fmt.Errorf("Internal error")
	}
	if _, err := io.WriteString(w, "{}"); err != nil {
		log.Println(err)
		return fmt.Errorf("Internal error")
	}

	return nil
}
