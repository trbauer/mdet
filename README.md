TODO:

- Save the motion frame with the output video so I can reproduce false positives and study the thresholds.
- Figure out the UI graph
   -- start offset for sample points is wrong
   -- graph goes beyond the bounds (to the right
   -- graph goes above the max value (clamp needed?)
   -- motion of 0 is above bottom part of frame
- Pre-buffer video looks wrong... (disabled for the moment)
- Fiddle with OpenCL support
- Draw the last motion sample value as text
- Print the analysis overhead