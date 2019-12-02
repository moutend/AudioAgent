package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"os/user"
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
	Commands []command `json:"commands"`
}

type postDefaultVoiceRequest struct {
	Index int32 `json:"index"`
}

type voiceProperty struct {
	Id          string `json:"id"`
	DisplayName string `json:"displayName"`
	Language    string `json:"language"`
}

type getVoicesResponse struct {
	DefaultVoiceIndex int32           `json:"defaultVoiceIndex"`
	VoiceProperties   []voiceProperty `json:"voiceProperties"`
}

const (
	enumPlay = 1
	enumWait = 2
	enumText = 3
	enumSSML = 4
)

var (
	logger  = log.New(ioutil.Discard, "discard logging messages", log.LstdFlags)
	logPath = ""
	dll     = syscall.NewLazyDLL("AudioNode.dll")

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
	if r.Method != "GET" {
		return
	}

	var code int32

	procFadeIn.Call(uintptr(unsafe.Pointer(&code)))

	if code != 0 {
		logger.Printf("Failed to call FadeIn() code=%d", code)
	}
}

func fadeOutHandler(w http.ResponseWriter, r *http.Request) {
	if r.Method != "GET" {
		return
	}

	var code int32

	procFadeOut.Call(uintptr(unsafe.Pointer(&code)))

	if code != 0 {
		logger.Printf("Failed to call FadeOut() code=%d", code)
	}
}

func commandHandler(w http.ResponseWriter, r *http.Request) {
	if r.Method != "POST" {
		return
	}

	buf := &bytes.Buffer{}

	if n, err := io.Copy(buf, r.Body); err != nil || n == 0 {
		logger.Fatalf("Failed to read request body")
	}

	var req postCommandRequest

	if err := json.Unmarshal(buf.Bytes(), &req); err != nil {
		logger.Fatal("Failed to execute json.Unmarshal()")
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
				continue
			}

			textU16Ptr, err := syscall.UTF16PtrFromString(text)

			if err != nil {
				logger.Fatal(err)
				continue
			}

			c.TextPtr = uintptr(unsafe.Pointer(textU16Ptr))
			c.TextLen = int32(len(textU16))
		case enumSSML:
			ssml := v.Value.(string)
			ssmlU16, err := syscall.UTF16FromString(ssml)

			if err != nil {
				logger.Fatal(err)
				continue
			}

			ssmlU16Ptr, err := syscall.UTF16PtrFromString(ssml)

			if err != nil {
				logger.Fatal(err)
			}

			c.SSMLPtr = uintptr(unsafe.Pointer(ssmlU16Ptr))
			c.SSMLLen = int32(len(ssmlU16))
		case enumWait:
			c.WaitDuration = v.Value.(float64)
		case enumPlay:
			c.SoundIndex = v.Value.(int16)
		default:
		}

		rawCommands[i] = uintptr(unsafe.Pointer(&c))
	}

	var code int32

	if len(rawCommands) > 0 {
		procFeed.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(&rawCommands[0])), uintptr(len(rawCommands)))
	}
	if code != 0 {
		logger.Printf("Failed to call Feed() code=%d", code)
	}
}

func voicesHandler(w http.ResponseWriter, r *http.Request) {
	if r.Method != "GET" {
		return
	}

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

	voiceProperties := []voiceProperty{}

	for i := int32(0); i < numberOfVoices; i++ {
		idLength := int32(0)

		procGetVoiceIdLength.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&idLength)))

		if code != 0 {
			logger.Printf("Failed to call GetVoiceIdLength() code=%d", code)
			continue
		}

		id := make([]uint16, idLength)

		procGetVoiceId.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&id[0])))

		if code != 0 {
			logger.Printf("Failed to call GetVoiceId() code=%d", code)
			continue
		}

		displayNameLength := int32(0)

		procGetVoiceDisplayNameLength.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&displayNameLength)))

		if code != 0 {
			logger.Printf("Failed to call GetVoiceDisplayNameLength() code=%d", code)
			continue
		}

		displayName := make([]uint16, displayNameLength)

		procGetVoiceDisplayName.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&displayName[0])))

		if code != 0 {
			logger.Printf("Failed to call GetVoiceDisplayName() code=%d", code)
			continue
		}

		languageLength := int32(0)

		procGetVoiceLanguageLength.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&languageLength)))

		if code != 0 {
			logger.Printf("Failed to call GetVoiceLanguageLength() code=%d", code)
			continue
		}

		language := make([]uint16, languageLength)

		procGetVoiceLanguage.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&language[0])))

		if code != 0 {
			logger.Printf("Failed to call GetVoiceLanguage() code=%d", code)
			continue
		}

		voiceProperties = append(voiceProperties, voiceProperty{
			Id:          syscall.UTF16ToString(id),
			DisplayName: syscall.UTF16ToString(displayName),
			Language:    syscall.UTF16ToString(language),
		})
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

func voiceHandler(w http.ResponseWriter, r *http.Request) {
	if r.Method != "POST" {
		return
	}

	buf := &bytes.Buffer{}

	if n, err := io.Copy(buf, r.Body); err != nil || n == 0 {
		logger.Fatal(err)
	}

	var req postDefaultVoiceRequest

	if err := json.Unmarshal(buf.Bytes(), &req); err != nil {
		logger.Fatal("Failed to call json.Unmarshal()")
		return
	}

	var code int32

	procSetDefaultVoice.Call(uintptr(unsafe.Pointer(&code)), uintptr(req.Index))

	if code != 0 {
		logger.Printf("Failed to call SetDefaultVoice() code=%d", code)
	}
}

func startHandler(w http.ResponseWriter, r *http.Request) {
	fullLogPath := filepath.Join(logPath, "AudioNode.ltsv")
	fullLogPathU16ptr, err := syscall.UTF16PtrFromString(fullLogPath)

	if err != nil {
		logger.Fatal(err)
	}

	var code int32

	procStart.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(fullLogPathU16ptr)), uintptr(0))

	if code != 0 {
		logger.Printf("Failed to call Start()")
	}
}

func quitHandler(w http.ResponseWriter, r *http.Request) {
	var code int32

	procQuit.Call(uintptr(unsafe.Pointer(&code)))

	if code != 0 {
		logger.Printf("Failed to call Quit()")
	}
}

func main() {
	file, err := os.Open("log.txt")

	if err != nil {
		panic(err)
	}

	logger = log.New(file, "", log.Lshortfile|log.LUTC)

	u, err := user.Current()

	if err != nil {
		log.Fatal(err)
	}

	logPath = filepath.Join(u.HomeDir, "AppData", "Roaming", "YetAnotherNarrater", "Log")
	os.MkdirAll(logPath, 0755)

	fmt.Println(logPath)

	mux := http.NewServeMux()

	mux.HandleFunc("/v1/engine/fadein", fadeInHandler)
	mux.HandleFunc("/v1/engine/fadeout", fadeOutHandler)
	mux.HandleFunc("/v1/engine/command", commandHandler)
	mux.HandleFunc("/v1/engine/voice", voiceHandler)
	mux.HandleFunc("/v1/engine/voices", voicesHandler)
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
