import numpy as np
from scipy.io import wavfile
from pydub import AudioSegment
from pydub.silence import split_on_silence
import os

def split_audio_file(input_file, min_silence_len=500, silence_thresh=-40):
    """
    Split an audio file into segments based on silence detection.
    
    Parameters:
    input_file (str): Path to the input audio file
    output_dir (str): Directory to save the split audio files
    min_silence_len (int): Minimum length of silence (in ms) to be considered as a split point
    silence_thresh (int): Sound level (in dB) to be considered as silence
    """
    output_dir="CREATE_AUDIO\split_audio"
    # Create output directory if it doesn't exist
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    # Load audio file
    audio = AudioSegment.from_file(input_file)
    
    # Split audio based on silence
    audio_chunks = split_on_silence(
        audio,
        min_silence_len=min_silence_len,
        silence_thresh=silence_thresh,
        keep_silence=100  # Keep 100ms of silence at the beginning and end
    )
    
    # Export each chunk
    for i, chunk in enumerate(audio_chunks):
        # Export chunk
        output_file = os.path.join(output_dir, f"{i+1}.wav")
        chunk.export(output_file, format="wav")
        
        # Print information about the chunk
        print(f"Saved segment {i+1}, duration: {len(chunk)/1000:.2f} seconds")

# Example usage
if __name__ == "__main__":
    split_audio_file(
        "CREATE_AUDIO\audio-tes.mp3",
        min_silence_len=100,    # Adjust this value based on your audio
        silence_thresh=-30      # Adjust this value based on your audio
    )