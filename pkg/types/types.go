package types

const (
	EnumSFX  = 1
	EnumWait = 2
	EnumText = 3
	EnumSSML = 4
)

type Command struct {
	Type         int16
	SFXIndex     int16
	WaitDuration uintptr
	Text         uintptr
}
