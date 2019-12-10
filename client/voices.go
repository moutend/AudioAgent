package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"math"
	"net/http"
	"strconv"
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

func getVoices(w http.ResponseWriter, r *http.Request) error {
	var code int32
	var numberOfVoices int32

	procGetVoiceCount.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(&numberOfVoices)))

	if code != 0 {
		logger.Printf("Failed to call GetVoiceCount() code=%d", code)
		return fmt.Errorf("Internal error")
	}

	var defaultVoiceIndex int32

	procGetDefaultVoice.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(&defaultVoiceIndex)))

	if code != 0 {
		logger.Printf("Failed to call GetDefaultVoice() code=%d", code)
		return fmt.Errorf("Internal error")
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

		var speakingRate uint64

		procGetSpeakingRate.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&speakingRate)))

		if code != 0 {
			logger.Printf("Failed to call GetSpeakingRate (code=%d)", code)
			break
		}

		var audioPitch uint64

		procGetAudioPitch.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&audioPitch)))

		if code != 0 {
			logger.Printf("Failed to call GetAudioPitch (code=%d)", code)
			break
		}

		var audioVolume uint64

		procGetAudioVolume.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&audioVolume)))

		if code != 0 {
			logger.Printf("Failed to call GetAudioVolume (code=%d)", code)
			break
		}

		voiceProperties[i].Id = syscall.UTF16ToString(id)
		voiceProperties[i].DisplayName = syscall.UTF16ToString(displayName)
		voiceProperties[i].Language = syscall.UTF16ToString(language)
		voiceProperties[i].SpeakingRate = math.Float64frombits(speakingRate)
		voiceProperties[i].AudioPitch = math.Float64frombits(audioPitch)
		voiceProperties[i].AudioVolume = math.Float64frombits(audioVolume)
	}
	if code != 0 {
		logger.Printf("Failed to obtain voice info (code=%v)", code)
		return fmt.Errorf("Internal error")
	}

	data, err := json.Marshal(getVoicesResponse{
		DefaultVoiceIndex: defaultVoiceIndex,
		Voices:            voiceProperties,
	})

	if err != nil {
		logger.Println(err)
		return fmt.Errorf("Internal error")
	}

	if _, err = io.Copy(w, bytes.NewBuffer(data)); err != nil {
		logger.Println(err)
		return fmt.Errorf("Internal error")
	}

	return nil
}

func putVoice(w http.ResponseWriter, r *http.Request) error {
	indexStr := r.URL.Query().Get("index")

	if indexStr == "" {
		err := fmt.Errorf("Query parameter 'index' is missing")

		logger.Println(err)
		return err
	}

	indexInt, err := strconv.Atoi(indexStr)

	if err != nil {
		logger.Println(err)
		return fmt.Errorf("Query parameter 'index' must be number")
	}

	voiceIndex := int32(indexInt)
	buf := &bytes.Buffer{}

	if _, err := io.Copy(buf, r.Body); err != nil {
		logger.Println(err)
		return fmt.Errorf("requested JSON is broken")
	}

	var req putVoiceRequest

	if err := json.Unmarshal(buf.Bytes(), &req); err != nil {
		logger.Println(err)
		return fmt.Errorf("Requested JSON is invalid")
	}

	var code int32

	if req.SpeakingRate >= 0.0 {
		procSetSpeakingRate.Call(uintptr(unsafe.Pointer(&code)), uintptr(voiceIndex), uintptr(math.Float64bits(req.SpeakingRate)))
	}
	if code != 0 {
		err := fmt.Errorf("Failed to call SetSpeakingRate(code=%v, index=%v, rate=%.2f)", code, voiceIndex, req.SpeakingRate)

		logger.Println(err)
		return err
	}
	if req.AudioPitch >= 0.0 {
		procSetAudioPitch.Call(uintptr(unsafe.Pointer(&code)), uintptr(voiceIndex), uintptr(math.Float64bits(req.AudioPitch)))
	}
	if code != 0 {
		err := fmt.Errorf("Failed to call SetAudioPitch (code=%v, index=%v, rate=%.2f)", code, voiceIndex, req.SpeakingRate)

		logger.Println(err)
		return err
	}
	if req.AudioVolume >= 0.0 {
		procSetAudioVolume.Call(uintptr(unsafe.Pointer(&code)), uintptr(voiceIndex), uintptr(math.Float64bits(req.AudioVolume)))
	}
	if code != 0 {
		err := fmt.Errorf("Failed to call SetAudioVolume (code=%v, index=%v, rate=%.2f)", code, voiceIndex, req.SpeakingRate)

		logger.Println(err)
		return err
	}
	if _, err := io.WriteString(w, "{}"); err != nil {
		logger.Println(err)
		return fmt.Errorf("Failed to write response")
	}

	return nil
}
