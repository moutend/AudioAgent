package api

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"math"
	"net/http"
	"strconv"
	"syscall"
	"unsafe"

	"github.com/moutend/AudioNode/pkg/dll"
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

func GetVoices(w http.ResponseWriter, r *http.Request) error {
	var code int32
	var numberOfVoices int32

	dll.ProcGetVoiceCount.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(&numberOfVoices)))

	if code != 0 {
		log.Printf("Failed to call GetVoiceCount() code=%d", code)
		return fmt.Errorf("Internal error")
	}

	var defaultVoiceIndex int32

	dll.ProcGetDefaultVoice.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(&defaultVoiceIndex)))

	if code != 0 {
		log.Printf("Failed to call GetDefaultVoice() code=%d", code)
		return fmt.Errorf("Internal error")
	}

	voiceProperties := make([]voiceProperty, numberOfVoices)

	for i, _ := range voiceProperties {
		var idLength int32

		dll.ProcGetVoiceIdLength.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&idLength)))

		if code != 0 {
			log.Printf("Failed to call GetVoiceIdLength() code=%d", code)
			break
		}

		id := make([]uint16, idLength)

		dll.ProcGetVoiceId.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&id[0])))

		if code != 0 {
			log.Printf("Failed to call GetVoiceId() code=%d", code)
			break
		}

		var displayNameLength int32

		dll.ProcGetVoiceDisplayNameLength.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&displayNameLength)))

		if code != 0 {
			log.Printf("Failed to call GetVoiceDisplayNameLength() code=%d", code)
			break
		}

		displayName := make([]uint16, displayNameLength)

		dll.ProcGetVoiceDisplayName.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&displayName[0])))

		if code != 0 {
			log.Printf("Failed to call GetVoiceDisplayName() code=%d", code)
			break
		}

		var languageLength int32

		dll.ProcGetVoiceLanguageLength.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&languageLength)))

		if code != 0 {
			log.Printf("Failed to call GetVoiceLanguageLength() code=%d", code)
			break
		}

		language := make([]uint16, languageLength)

		dll.ProcGetVoiceLanguage.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&language[0])))

		if code != 0 {
			log.Printf("Failed to call GetVoiceLanguage() code=%d", code)
			break
		}

		var speakingRate uint64

		dll.ProcGetSpeakingRate.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&speakingRate)))

		if code != 0 {
			log.Printf("Failed to call GetSpeakingRate (code=%d)", code)
			break
		}

		var audioPitch uint64

		dll.ProcGetAudioPitch.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&audioPitch)))

		if code != 0 {
			log.Printf("Failed to call GetAudioPitch (code=%d)", code)
			break
		}

		var audioVolume uint64

		dll.ProcGetAudioVolume.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&audioVolume)))

		if code != 0 {
			log.Printf("Failed to call GetAudioVolume (code=%d)", code)
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
		log.Printf("Failed to obtain voice info (code=%v)", code)
		return fmt.Errorf("Internal error")
	}

	data, err := json.Marshal(getVoicesResponse{
		DefaultVoiceIndex: defaultVoiceIndex,
		Voices:            voiceProperties,
	})

	if err != nil {
		log.Println(err)
		return fmt.Errorf("Internal error")
	}

	if _, err = io.Copy(w, bytes.NewBuffer(data)); err != nil {
		log.Println(err)
		return fmt.Errorf("Internal error")
	}

	return nil
}

func PostVoice(w http.ResponseWriter, r *http.Request) error {
	indexStr := r.URL.Query().Get("index")

	if indexStr == "" {
		err := fmt.Errorf("Query parameter 'index' is missing")

		log.Println(err)
		return err
	}

	indexInt, err := strconv.Atoi(indexStr)

	if err != nil {
		log.Println(err)
		return fmt.Errorf("Query parameter 'index' must be number")
	}

	voiceIndex := int32(indexInt)
	buf := &bytes.Buffer{}

	if _, err := io.Copy(buf, r.Body); err != nil {
		log.Println(err)
		return fmt.Errorf("requested JSON is broken")
	}

	var req putVoiceRequest

	if err := json.Unmarshal(buf.Bytes(), &req); err != nil {
		log.Println(err)
		return fmt.Errorf("Requested JSON is invalid")
	}

	var code int32

	if req.SpeakingRate >= 0.0 {
		dll.ProcSetSpeakingRate.Call(uintptr(unsafe.Pointer(&code)), uintptr(voiceIndex), uintptr(math.Float64bits(req.SpeakingRate)))
	}
	if code != 0 {
		err := fmt.Errorf("Failed to call SetSpeakingRate(code=%v, index=%v, rate=%.2f)", code, voiceIndex, req.SpeakingRate)

		log.Println(err)
		return err
	}
	if req.AudioPitch >= 0.0 {
		dll.ProcSetAudioPitch.Call(uintptr(unsafe.Pointer(&code)), uintptr(voiceIndex), uintptr(math.Float64bits(req.AudioPitch)))
	}
	if code != 0 {
		err := fmt.Errorf("Failed to call SetAudioPitch (code=%v, index=%v, rate=%.2f)", code, voiceIndex, req.SpeakingRate)

		log.Println(err)
		return err
	}
	if req.AudioVolume >= 0.0 {
		dll.ProcSetAudioVolume.Call(uintptr(unsafe.Pointer(&code)), uintptr(voiceIndex), uintptr(math.Float64bits(req.AudioVolume)))
	}
	if code != 0 {
		err := fmt.Errorf("Failed to call SetAudioVolume (code=%v, index=%v, rate=%.2f)", code, voiceIndex, req.SpeakingRate)

		log.Println(err)
		return err
	}
	if _, err := io.WriteString(w, "{}"); err != nil {
		log.Println(err)
		return fmt.Errorf("Failed to write response")
	}

	return nil
}

func PostVoiceRate(w http.ResponseWriter, r *http.Request) error {
	diffStr := r.URL.Query().Get("diff")

	if diffStr == "" {
		err := fmt.Errorf("Query parameter 'diff' is missing")

		log.Println(err)
		return err
	}

	diffFloat64, err := strconv.ParseFloat(diffStr, 64)

	if err != nil {
		log.Println(err)
		return fmt.Errorf("Query parameter 'index' must be number")
	}
	if diffFloat64 == 0.0 {
		return nil
	}

	var code int32
	var voiceIndex int32

	dll.ProcGetDefaultVoice.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(&voiceIndex)))

	if code != 0 {
		err := fmt.Errorf("Failed to call GetDefaultVoice (code=%v)", code)

		log.Println(err)
		return err
	}

	var speakingRate uint64

	dll.ProcGetSpeakingRate.Call(uintptr(unsafe.Pointer(&code)), uintptr(voiceIndex), uintptr(unsafe.Pointer(&speakingRate)))

	if code != 0 {
		err := fmt.Errorf("Failed to call GetSpeakingRate (code=%v, index=%v)", code, voiceIndex)

		log.Println(err)
		return err
	}

	newSpeakingRate := diffFloat64 + math.Float64frombits(speakingRate)

	dll.ProcSetSpeakingRate.Call(uintptr(unsafe.Pointer(&code)), uintptr(voiceIndex), uintptr(math.Float64bits(newSpeakingRate)))

	if code != 0 {
		err := fmt.Errorf("Failed to call SetSpeakingRate (code=%v, index=%v, rate=%.2f)", code, voiceIndex, newSpeakingRate)

		log.Println(err)
		return err
	}
	if _, err := io.WriteString(w, "{}"); err != nil {
		log.Println(err)
		return fmt.Errorf("Failed to write response")
	}

	return nil
}

func PostVoicePitch(w http.ResponseWriter, r *http.Request) error {
	diffStr := r.URL.Query().Get("diff")

	if diffStr == "" {
		err := fmt.Errorf("Query parameter 'diff' is missing")

		log.Println(err)
		return err
	}

	diffFloat64, err := strconv.ParseFloat(diffStr, 64)

	if err != nil {
		log.Println(err)
		return fmt.Errorf("Query parameter 'index' must be number")
	}
	if diffFloat64 == 0.0 {
		return nil
	}

	var code int32
	var voiceIndex int32

	dll.ProcGetDefaultVoice.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(&voiceIndex)))

	if code != 0 {
		err := fmt.Errorf("Failed to call GetDefaultVoice (code=%v)", code)

		log.Println(err)
		return err
	}

	var speakingPitch uint64

	dll.ProcGetAudioPitch.Call(uintptr(unsafe.Pointer(&code)), uintptr(voiceIndex), uintptr(unsafe.Pointer(&speakingPitch)))

	if code != 0 {
		err := fmt.Errorf("Failed to call GetAudioPitch (code=%v, index=%v)", code, voiceIndex)

		log.Println(err)
		return err
	}

	newSpeakingPitch := diffFloat64 + math.Float64frombits(speakingPitch)

	dll.ProcSetAudioPitch.Call(uintptr(unsafe.Pointer(&code)), uintptr(voiceIndex), uintptr(math.Float64bits(newSpeakingPitch)))

	if code != 0 {
		err := fmt.Errorf("Failed to call SetAudioPitch (code=%v, index=%v, audioPitch=%.2f)", code, voiceIndex, newSpeakingPitch)

		log.Println(err)
		return err
	}
	if _, err := io.WriteString(w, "{}"); err != nil {
		log.Println(err)
		return fmt.Errorf("Failed to write response")
	}

	return nil
}

func PostVoiceVolume(w http.ResponseWriter, r *http.Request) error {
	diffStr := r.URL.Query().Get("diff")

	if diffStr == "" {
		err := fmt.Errorf("Query parameter 'diff' is missing")

		log.Println(err)
		return err
	}

	diffFloat64, err := strconv.ParseFloat(diffStr, 64)

	if err != nil {
		log.Println(err)
		return fmt.Errorf("Query parameter 'index' must be number")
	}
	if diffFloat64 == 0.0 {
		return nil
	}

	var code int32
	var voiceIndex int32

	dll.ProcGetDefaultVoice.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(&voiceIndex)))

	if code != 0 {
		err := fmt.Errorf("Failed to call GetDefaultVoice (code=%v)", code)

		log.Println(err)
		return err
	}

	var speakingVolume uint64

	dll.ProcGetAudioVolume.Call(uintptr(unsafe.Pointer(&code)), uintptr(voiceIndex), uintptr(unsafe.Pointer(&speakingVolume)))

	if code != 0 {
		err := fmt.Errorf("Failed to call GetAudioVolume (code=%v, index=%v)", code, voiceIndex)

		log.Println(err)
		return err
	}

	newSpeakingVolume := diffFloat64 + math.Float64frombits(speakingVolume)

	dll.ProcSetAudioVolume.Call(uintptr(unsafe.Pointer(&code)), uintptr(voiceIndex), uintptr(math.Float64bits(newSpeakingVolume)))

	if code != 0 {
		err := fmt.Errorf("Failed to call SetAudioVolume (code=%v, index=%v, audioVolume=%.2f)", code, voiceIndex, newSpeakingVolume)

		log.Println(err)
		return err
	}
	if _, err := io.WriteString(w, "{}"); err != nil {
		log.Println(err)
		return fmt.Errorf("Failed to write response")
	}

	return nil
}
