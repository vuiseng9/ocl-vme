To create example test input:

1. download some content (Big Buck Bunny will work.  Default resolution for the samples is 1280x720)
http://download.blender.org/peach/bigbuckbunny_movies/big_buck_bunny_720p_h264.mov

2. demux and convert to yuv with ffmpeg:
ffmpeg -i big_buck_bunny_720p_h264.mov -ss 00:02:00 -vframes 500 bbb_1280x720.yuv

