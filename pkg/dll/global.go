package dll

import (
	"syscall"
)

var (
	dll = syscall.NewLazyDLL("AudioNode.dll")

	ProcSetup                     = dll.NewProc("Setup")
	ProcTeardown                  = dll.NewProc("Teardown")
	ProcFadeIn                    = dll.NewProc("FadeIn")
	ProcFadeOut                   = dll.NewProc("FadeOut")
	ProcPush                      = dll.NewProc("Push")
	ProcGetVoiceCount             = dll.NewProc("GetVoiceCount")
	ProcGetVoiceId                = dll.NewProc("GetVoiceId")
	ProcGetVoiceIdLength          = dll.NewProc("GetVoiceIdLength")
	ProcGetVoiceDisplayName       = dll.NewProc("GetVoiceDisplayName")
	ProcGetVoiceDisplayNameLength = dll.NewProc("GetVoiceDisplayNameLength")
	ProcGetVoiceLanguage          = dll.NewProc("GetVoiceLanguage")
	ProcGetVoiceLanguageLength    = dll.NewProc("GetVoiceLanguageLength")
	ProcGetDefaultVoice           = dll.NewProc("GetDefaultVoice")
	ProcSetDefaultVoice           = dll.NewProc("SetDefaultVoice")
	ProcGetSpeakingRate           = dll.NewProc("GetSpeakingRate")
	ProcSetSpeakingRate           = dll.NewProc("SetSpeakingRate")
	ProcGetAudioPitch             = dll.NewProc("GetAudioPitch")
	ProcSetAudioPitch             = dll.NewProc("SetAudioPitch")
	ProcGetAudioVolume            = dll.NewProc("GetAudioVolume")
	ProcSetAudioVolume            = dll.NewProc("SetAudioVolume")
)
