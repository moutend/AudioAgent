package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"os/user"
	"path/filepath"
	"syscall"
	"unsafe"
)

type Command struct {
	Type         int16
	SoundIndex   int16
	WaitDuration float64
	SSMLLen      int32
	SSMLPtr      uintptr
}

type postDefaultVoiceRequest struct {
	Index int `json:"index"`
}

type getVoicesResponse struct {
	DefaultVoiceIndex int             `json:"defaultVoiceIndex"`
	VoiceProperties   []VoiceProperty `json:"voiceProperties"`
}

type VoiceProperty struct {
	Id          string `json:"id"`
	DisplayName string `json:"displayName"`
	Language    string `json:"language"`
}

var (
	dll = syscall.NewLazyDLL("AudioNode.dll")

	procStart                     = dll.NewProc("Start")
	procQuit                      = dll.NewProc("Quit")
	procFadeIn                    = dll.NewProc("FadeIn")
	procFadeOut                   = dll.NewProc("FadeOut")
	procFeed                      = dll.NewProc("Feed")
	procGetVoiceCount             = dll.NewProc("GetVoiceCount")
	procGetVoiceId                = dll.NewProc("GetVoiceId")
	procGetVoiceIdLength          = dll.NewProc("GetVoiceIdLength")
	procGetVoiceDisplayName       = dll.NewProc("GetVoiceDisplayName")
	procGetVoiceDisplayNameLength = dll.NewProc("GetVoiceDisplayNameLength")
	procGetVoiceLanguage          = dll.NewProc("GetVoiceLanguage")
	procGetVoiceLanguageLength    = dll.NewProc("GetVoiceLanguageLength")
	procGetDefaultVoice           = dll.NewProc("GetDefaultVoice")
	procSetDefaultVoice           = dll.NewProc("SetDefaultVoice")
)

func fadeInHandler(w http.ResponseWriter, r *http.Request) {
	code := int32(0)

	procFadeIn.Call(uintptr(unsafe.Pointer(&code)))

	if code != 0 {
		log.Printf("Failed to call FadeIn() code=%d", code)
	}
}

func fadeOutHandler(w http.ResponseWriter, r *http.Request) {
	code := int32(0)

	procFadeOut.Call(uintptr(unsafe.Pointer(&code)))

	if code != 0 {
		log.Printf("Failed to call FadeOut() code=%d", code)
	}
}

func feedHandler(w http.ResponseWriter, r *http.Request) {
	buf := &bytes.Buffer{}
	if _, err := io.Copy(buf, r.Body); err != nil {
		fmt.Printf("error while reading request body (%v)\n", err)
	}
	tts := buf.String()
	if tts == "" {
		tts = "placeholder"
	}
	ssml := fmt.Sprintf("<speak version='1.0' xmlns='http://www.w3.org/2001/10/synthesis' xml:lang='en-US'>%s</speak>", tts)

	text, err := syscall.UTF16FromString(ssml)
	if err != nil {
		log.Println(err)
	}

	textPtr, err := syscall.UTF16PtrFromString(ssml)
	if err != nil {
		log.Println(err)
	}

	textLength := int32(len(text))

	code := int32(0)

	cmds := make([]uintptr, 2, 2)

	cmd1 := Command{
		Type:       1,
		SoundIndex: 0,
	}
	cmds[0] = uintptr(unsafe.Pointer(&cmd1))

	cmd2 := Command{
		Type:    3,
		SSMLPtr: uintptr(unsafe.Pointer(textPtr)),
		SSMLLen: textLength,
	}
	cmds[1] = uintptr(unsafe.Pointer(&cmd2))
	procFeed.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(&cmds[0])), uintptr(2))

	if code != 0 {
		log.Printf("Failed to call Feed() code=%d", code)
	}
}

func voicesHandler(w http.ResponseWriter, r *http.Request) {
	code := int32(0)
	numberOfVoices := int32(0)

	procGetVoiceCount.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(&numberOfVoices)))

	if code != 0 {
		log.Printf("Failed to call GetVoiceCount() code=%d", code)
	}

	defaultVoiceIndex := int32(0)
	procGetDefaultVoice.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(&defaultVoiceIndex)))

	if code != 0 {
		log.Printf("Failed to call GetDefaultVoice() code=%d", code)
	}

	voiceProperties := []VoiceProperty{}

	for i := int32(0); i < numberOfVoices; i++ {
		idLength := int32(0)

		procGetVoiceIdLength.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&idLength)))

		if code != 0 {
			log.Printf("Failed to call GetVoiceIdLength() code=%d", code)
			continue
		}

		id := make([]uint16, idLength)

		procGetVoiceId.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&id[0])))

		if code != 0 {
			log.Printf("Failed to call GetVoiceId() code=%d", code)
			continue
		}

		displayNameLength := int32(0)

		procGetVoiceDisplayNameLength.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&displayNameLength)))

		if code != 0 {
			log.Printf("Failed to call GetVoiceDisplayNameLength() code=%d", code)
			continue
		}

		displayName := make([]uint16, displayNameLength)

		procGetVoiceDisplayName.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&displayName[0])))

		if code != 0 {
			log.Printf("Failed to call GetVoiceDisplayName() code=%d", code)
			continue
		}

		languageLength := int32(0)

		procGetVoiceLanguageLength.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&languageLength)))

		if code != 0 {
			log.Printf("Failed to call GetVoiceLanguageLength() code=%d", code)
			continue
		}

		language := make([]uint16, languageLength)

		procGetVoiceLanguage.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&language[0])))

		if code != 0 {
			log.Printf("Failed to call GetVoiceLanguage() code=%d", code)
			continue
		}

		voiceProperties = append(voiceProperties, VoiceProperty{
			Id:          syscall.UTF16ToString(id),
			DisplayName: syscall.UTF16ToString(displayName),
			Language:    syscall.UTF16ToString(language),
		})
	}

	data, err := json.Marshal(getVoicesResponse{
		DefaultVoiceIndex: int(defaultVoiceIndex),
		VoiceProperties:   voiceProperties,
	})
	if err != nil {
		log.Fatal("failed to call json.Marshal()")
		return
	}

	io.Copy(w, bytes.NewBuffer(data))
}

func voiceHandler(w http.ResponseWriter, r *http.Request) {
	req := postDefaultVoiceRequest{}
	buf := &bytes.Buffer{}
	io.Copy(buf, r.Body)

	err := json.Unmarshal(buf.Bytes(), &req)

	if err != nil {
		log.Fatal("Failed to call json.Unmarshal()")
		return
	}

	code := int32(0)
	index := int32(req.Index)
	procSetDefaultVoice.Call(uintptr(unsafe.Pointer(&code)), uintptr(index))

	if code != 0 {
		log.Printf("Failed to call SetDefaultVoice() code=%d", code)
	}
}

func setDefaultVoiceHandler(w http.ResponseWriter, r *http.Request) {
	code := int32(0)
	index := int32(0)
	procSetDefaultVoice.Call(uintptr(unsafe.Pointer(&code)), uintptr(index))

	if code != 0 {
		log.Printf("Failed to call SetDefaultVoiceIndex() code=%d", code)
	}
}

func startHandler(w http.ResponseWriter, r *http.Request) {
	u, err := user.Current()

	if err != nil {
		fmt.Println(err)
	}

	fullLogPath := filepath.Join(u.HomeDir, "AppData", "Roaming", "YetAnotherNarrater", "Log", "AudioAgent.ltsv")

	os.MkdirAll(filepath.Dir(fullLogPath), 0755)

	fmt.Println(fullLogPath)
	u16ptr, err := syscall.UTF16PtrFromString(fullLogPath)

	if err != nil {
		fmt.Println(err)
		return
	}
	code := int32(0)

	procStart.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(u16ptr)), uintptr(0))

	if code != 0 {
		log.Printf("Failed to call Start()")
	}
}

func quitHandler(w http.ResponseWriter, r *http.Request) {
	code := int32(0)

	procQuit.Call(uintptr(unsafe.Pointer(&code)))

	if code != 0 {
		log.Printf("Failed to call Quit()")
	}
}

func main() {
	mux := http.NewServeMux()

	mux.HandleFunc("/v1/engine/fadein", fadeInHandler)
	mux.HandleFunc("/v1/engine/fadeout", fadeOutHandler)
	mux.HandleFunc("/v1/engine/feed", feedHandler)
	mux.HandleFunc("/v1/engine/voices", voicesHandler)
	mux.HandleFunc("/v1/engine/voice", voicesHandler)
	mux.HandleFunc("/v1/engine/start", startHandler)
	mux.HandleFunc("/v1/engine/quit", quitHandler)

	server := &http.Server{
		Addr:    ":4000",
		Handler: mux,
	}

	if err := server.ListenAndServe(); err != nil {
		panic(err)
	}

	defer server.Close()
}
