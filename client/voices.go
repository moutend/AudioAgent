package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"math"
	"net/http"
	"strings"
	"syscall"
	"unsafe"
)

type voiceProperty struct {
	rVoiceProperty
	rwVoiceProperty
}

type rVoiceProperty struct {
	Id          string `json:"id"`
	DisplayName string `json:"displayName"`
	Language    string `json:"language"`
}

type rwVoiceProperty struct {
	SpeakingRate float64 `json:"speakingRate"`
	AudioPitch   float64 `json:"audioPitch"`
	AudioVolume  float64 `json:"audioVolume"`
}

type putVoiceRequest rwVoiceProperty

func voicesHandler(w http.ResponseWriter, r *http.Request) {
	voiceId := ""

	if ps := strings.Split(r.URL.Path, "/"); len(ps) > 2 {
		voiceId = ps[2]
	}
	if voiceId == "" {
		processVoices(w, r)
	} else {
		processVoice(voiceId, w, r)
	}
}

func processVoices(w http.ResponseWriter, r *http.Request) {
	if r.Method != "GET" {
		return
	}
	getVoices(w, r)
}

func getVoices(w http.ResponseWriter, r *http.Request) {
	var code int32
	var numberOfVoices int32

	procGetVoiceCount.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(&numberOfVoices)))

	if code != 0 {
		logger.Printf("Failed to call GetVoiceCount() code=%d", code)
	}

	var defaultVoiceIndex int32

	procGetDefaultVoice.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(&defaultVoiceIndex)))

	if code != 0 {
		logger.Printf("Failed to call GetDefaultVoice() code=%d", code)
	}

	voiceProperties := make([]voiceProperty, numberOfVoices)

	for i, _ := range voiceProperties {
		var idLength int32

		procGetVoiceIdLength.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&idLength)))

		if code != 0 {
			logger.Printf("Failed to call GetVoiceIdLength() code=%d", code)
			break
		}

		id := make([]uint16, idLength)

		procGetVoiceId.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&id[0])))

		if code != 0 {
			logger.Printf("Failed to call GetVoiceId() code=%d", code)
			break
		}

		var displayNameLength int32

		procGetVoiceDisplayNameLength.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&displayNameLength)))

		if code != 0 {
			logger.Printf("Failed to call GetVoiceDisplayNameLength() code=%d", code)
			break
		}

		displayName := make([]uint16, displayNameLength)

		procGetVoiceDisplayName.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&displayName[0])))

		if code != 0 {
			logger.Printf("Failed to call GetVoiceDisplayName() code=%d", code)
			break
		}

		var languageLength int32

		procGetVoiceLanguageLength.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&languageLength)))

		if code != 0 {
			logger.Printf("Failed to call GetVoiceLanguageLength() code=%d", code)
			break
		}

		language := make([]uint16, languageLength)

		procGetVoiceLanguage.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&language[0])))

		if code != 0 {
			logger.Printf("Failed to call GetVoiceLanguage() code=%d", code)
			break
		}

		voiceProperties[i] = voiceProperty{
			Id:           syscall.UTF16ToString(id),
			DisplayName:  syscall.UTF16ToString(displayName),
			Language:     syscall.UTF16ToString(language),
			SpeakingRate: speakingRate,
			AudioPitch:   audioPitch,
			AudioVolume:  audioVolume,
		}
	}
	if code != 0 {
		// todo
	}

	data, err := json.Marshal(getVoicesResponse{
		DefaultVoiceIndex: defaultVoiceIndex,
		VoiceProperties:   voiceProperties,
	})

	if err != nil {
		logger.Fatal("failed to call json.Marshal()")
	}

	io.Copy(w, bytes.NewBuffer(data))
}

func processVoice(voiceIndex int32, w http.ResponseWriter, r *http.Request) error {
	switch r.Method {
	case "PUT":
		return putVoice(voiceId, w, r)
	default:
	}

	return fmt.Errorf("Invalid HTTP method: %v", r.Method)
}

func putVoice(voiceIndex int32, w http.ResponseWriter, r *http.Request) error {
	buf := &bytes.Buffer{}

	if n, err := io.Copy(buf, r.Body); err != nil || n == 0 {
		if err != nil {
			logger.fatal(err)
		} else {
			logger.Fatalf("Failed to read request body")
		}

		return fmt.Errorf("requested JSON is empty or broken")
	}

	var req putVoiceRequest

	if err := json.Unmarshal(buf.Bytes(), &req); err != nil {
		logger.Fatal(err)

		return fmt.Errorf("Requested JSON is invalid")
	}

	var code int32

	if req.SpeakingRate != 0.0 {
		procSetSpeakingRate.Call(uintptr(unsafe.Pointer(&code)), uintptr(voiceIndex), uintptr(math.Float64bits(req.SpeakingRate)))
	}
	if code != 0 {
		logger.Fatalf("Failed to call SetSpeakingRate(code=%v, index=%v, rate=%.2f)", code, voiceIndex, req.SpeakingRate)

		return fmt.Errorf("Internal error: Failed to call SetSpeakingRate")
	}

	return nil
}
