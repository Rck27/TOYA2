from pydub import AudioSegment
from IPython.display import Audio, display
import sys
import zipfile

def archive_files_with_prefix(folder_path, prefixes, output_zip):
    with zipfile.ZipFile(output_zip, 'w') as zipf:
        for file in os.listdir(folder_path):
            if any(file.startswith(prefix) for prefix in prefixes):
                zipf.write(os.path.join(folder_path, file), file)
    print(f"Archived files into {output_zip}")

def convert_to_pcm(input_file, output_pcm):
    try:
        # Load audio file (supports MP3, WAV, and other formats)
        audio = AudioSegment.from_file(input_file)

        # Convert to mono (left channel only)
        audio = audio.set_channels(1)

        # Set frame rate to 8kHz
        audio = audio.set_frame_rate(16000)

        # Convert to 8-bit
        audio = audio.set_sample_width(2)

        # Export as raw PCM
        audio.export(output_pcm, format='raw')

        print(f"Successfully converted {input_file} to {output_pcm}")
        print("Converted to: 16kHz, 16bit, mono (left channel)")

    except Exception as e:
        print(f"Error converting file: {str(e)}")

# if __name__ == "__main__":
#     if len(sys.argv) != 3:
#         print("Usage: python script.py input_audio output.pcm")
#         print("Supported input formats: MP3, WAV, OGG, FLAC, etc.")
#         sys.exit(1)

#     convert_to_pcm(sys.argv[1], sys.argv[2])


import os, glob
path = '/content/'
for filename in glob.glob("/content/*.mp3"):
  # print(filename)
  convert_to_pcm(filename, f"{filename.replace('.mp3', '')}.pcm")
archive_files_with_prefix("/content/", ".pcm", "out.zip")

  #  with open(os.path.join(os.getcwd(), filename), 'r') as f: # open in readonly mode
      # do your stuff