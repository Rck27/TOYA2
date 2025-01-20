from pydub import AudioSegment
import sys

def convert_to_pcm(input_file, output_pcm):
    try:
        # Load audio file (supports MP3, WAV, and other formats)
        audio = AudioSegment.from_file(input_file)
        
        # Convert to mono (left channel only)
        audio = audio.set_channels(1)
        
        # Set frame rate to 8kHz
        audio = audio.set_frame_rate(8000)
        
        # Convert to 8-bit
        audio = audio.set_sample_width(1)
        
        # Export as raw PCM
        audio.export(output_pcm, format='raw')
        
        print(f"Successfully converted {input_file} to {output_pcm}")
        print("Converted to: 8kHz, 8bit, mono (left channel)")
            
    except Exception as e:
        print(f"Error converting file: {str(e)}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python script.py input_audio output.pcm")
        print("Supported input formats: MP3, WAV, OGG, FLAC, etc.")
        sys.exit(1)
        
    convert_to_pcm(sys.argv[1], sys.argv[2])