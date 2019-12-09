package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"math"
	"net/http"
	"strconv"
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

type getVoicesResponse struct {
	DefaultVoiceIndex int32           `json:"defaultVoiceIndex"`
	Voices            []voiceProperty `json:"voices"`
}

type putVoiceRequest rwVoiceProperty

func voicesHandler(w http.ResponseWriter, r *http.Request) {
	var err error

	defer func() {
		w.Header().Set("Content-Type", jsonContentType)

		if err == nil {
			return
		}

		data, _ := json.Marshal(struct {
			Error string `json:"error"`
		}{
			Error: err.Error(),
		})

		io.Copy(w, bytes.NewBuffer(data))

		w.WriteHeader(http.StatusBadRequest)
	}()

	voiceIndex := ""

	if ps := strings.Split(r.URL.Path, "/"); len(ps) > 4 {
		voiceIndex = ps[3]
	}
	if voiceIndex == "" {
		err = processVoices(w, r)
		return
	}

	var i int

	if i, err = strconv.Atoi(voiceIndex); err != nil {
		return
	}

	err = processVoice(int32(i), w, r)
}

func processVoices(w http.ResponseWriter, r *http.Request) error {
	if r.Method != "GET" {
		return fmt.Errorf("Invalid HTTP method: %v", r.Method)
	}

	return getVoices(w, r)
}

func getVoices(w http.ResponseWriter, r *http.Request) error {
	var code int32
	var numberOfVoices int32

	procGetVoiceCount.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(&numberOfVoices)))

	if code != 0 {
		logger.Printf("Failed to call GetVoiceCount() code=%d", code)
		return fmt.Errorf("Internal error (code=%v)", code)
	}

	var defaultVoiceIndex int32

	procGetDefaultVoice.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(&defaultVoiceIndex)))

	if code != 0 {
		logger.Printf("Failed to call GetDefaultVoice() code=%d", code)
		return fmt.Errorf("Internal error (code=%v)", code)
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

		speakingRate := 0.0
		audioPitch := 0.0
		audioVolume := 0.0

		voiceProperties[i].Id = syscall.UTF16ToString(id)
		voiceProperties[i].DisplayName = syscall.UTF16ToString(displayName)
		voiceProperties[i].Language = syscall.UTF16ToString(language)
		voiceProperties[i].SpeakingRate = speakingRate
		voiceProperties[i].AudioPitch = audioPitch
		voiceProperties[i].AudioVolume = audioVolume
	}
	if code != 0 {
		logger.Fatalf("Failed to obtain voice info (code=%v)", code)
		return fmt.Errorf("Internal error (code=%v)", code)
	}

	data, err := json.Marshal(getVoicesResponse{
		DefaultVoiceIndex: defaultVoiceIndex,
		Voices:            voiceProperties,
	})

	if err != nil {
		logger.Fatal(err)
		return err
	}

	if _, err = io.Copy(w, bytes.NewBuffer(data)); err != nil {
		logger.Fatal(err)
		return fmt.Errorf("Internal error")
	}

	return nil
}

func processVoice(voiceIndex int32, w http.ResponseWriter, r *http.Request) error {
	switch r.Method {
	case "PUT":
		return putVoice(voiceIndex, w, r)
	default:
	}

	return fmt.Errorf("Invalid HTTP method: %v", r.Method)
}

func putVoice(voiceIndex int32, w http.ResponseWriter, r *http.Request) error {
	buf := &bytes.Buffer{}

	if n, err := io.Copy(buf, r.Body); err != nil || n == 0 {
		if err != nil {
			logger.Fatal(err)
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
