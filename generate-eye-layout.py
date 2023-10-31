import csv
import numpy as np
import math

rotation_angle = -12 # degrees. positive values rotate CCW when looking at the left eye.
rotation_angle_radians = (rotation_angle * np.pi) / 180.0

# Mapping of port pins to logical channels
channelSwizzle = [4, 0, 5, 1, 6, 2, 7, 3]

def read_csv(filename):
  with open(filename, "r") as f:
    reader = csv.DictReader(f)
    channelPoints = []
    points = []
    layer = None

    for row in reader:
      designator = row['Designator']
      if (designator[0] != 'D'):
        continue
      if layer is None:
        layer = row['Layer']

      idx = int(designator[1:None]);
      channel = idx // 100
      while (len(channelPoints) <= channel):
        channelPoints.append([])

      channelPoints[channel].append(np.array([float(row['Mid X']), float(row['Mid Y'])]))
    compactedChannelPoints = []
    for channel in channelPoints:
      if (len(channel) > 0):
        compactedChannelPoints.append(np.stack(channel))
    return compactedChannelPoints, layer

def bounds(channelPoints):
  allPoints = np.concatenate(channelPoints)
  boundsMin = np.amin(allPoints, axis=0)
  boundsMax = np.amax(allPoints, axis=0)
  return boundsMin, boundsMax

def rotate_point(p, center):
  d = p - center
  cosa = math.cos(rotation_angle_radians)
  sina = math.sin(rotation_angle_radians)

  # 2d rotation matrix: [[cos, sin], [-sin, cos]] * [x, y]
  rp = center + np.array([np.dot(d, np.array([cosa, sina])), np.dot(d, np.array([-sina, cosa]))])

  #print(f"{p} => {rp}")
  return rp


left, left_layer = read_csv("Shadowfox-Eye-Left-pos.csv")
right, right_layer = read_csv("Shadowfox-Eye-Right-pos.csv")



# Flip the right eye to match the left if they're on the same layer.
# (there two possible mirroring strategies: moving all components to the opposite layer
# does not require coordinate system fixup, while flipping components on the same layer will
# require coordinate system fixup.
right_flip = (right_layer == left_layer)

left_min, left_max = bounds(left)
right_min, right_max = bounds(right)

left_center = (left_min + left_max) * 0.5
right_center = (right_min + right_max) * 0.5


channelMaxLength = max(max(len(channel) for channel in left), max(len(channel) for channel in right))

print(f"left_min = {left_min}")
print(f"left_max = {left_max}")
print(f"left_center = {left_center}")
print(f"right_min = {right_min}")
print(f"right_max = {right_max}")
print(f"right_center = {right_center}")
print(f"channelMaxLength = {channelMaxLength}")



with open("Inc/eye_layout.h", "w", encoding="utf-8") as f:

  f.write(f"""
#pragma once
#include <glm/vec2.hpp>

extern const glm::vec2 channelPoints[];
extern const uint16_t channelPointOffsets[];
extern const uint8_t channelLengths[];
extern const uint8_t channelEyeIndices[];
static constexpr uint8_t channelMaxLength = {channelMaxLength};
extern const glm::vec2 boundsMin, boundsMax, boundsCenter;

""")

with open("Src/eye_layout.cpp", "w", encoding="utf-8") as f:
  f.write('#include "eye_layout.h"\n\n')
  f.write('#include <glm/vec2.hpp>\n\n')
  f.write('using glm::vec2;')

  channelOffsets = []
  channelLengths = []
  channelEyeIndices = []
  currentOffset = 0

  rotatedLeftPoints = []
  rotatedRightPoints = []

  f.write("const vec2 channelPoints[] = {\n")
  for channel in left:
    channelOffsets.append(str(currentOffset));
    channelLengths.append(str(len(channel)))
    channelEyeIndices.append(str(0));
    for point in channel:
      p = rotate_point(point, left_center)
      rotatedLeftPoints.append(p)

      f.write(  f"  vec2({p[0]}f, {p[1]}f),\n")
    currentOffset += len(channel)

  for channel in right:
    channelOffsets.append(str(currentOffset));
    channelLengths.append(str(len(channel)))
    channelEyeIndices.append(str(1));
    for point in channel:
      p = rotate_point(point, right_center)
      if right_flip:
        # Flip right points and align them with left points in X, if requested
        p[0] = (right_max[0] - p[0]) + left_min[0]

      rotatedRightPoints.append(p)
      f.write(  f"  vec2({p[0]}f, {p[1]}f),\n")
    currentOffset += len(channel)

  f.write("};\n")

  rotatedLeftPoints = np.stack(rotatedLeftPoints)
  rotatedRightPoints = np.stack(rotatedRightPoints)


  leftAdjMin = np.amin(rotatedLeftPoints, axis=0)
  leftAdjMax = np.amax(rotatedLeftPoints, axis=0)

  rightAdjMin = np.amin(rotatedRightPoints, axis=0)
  rightAdjMax = np.amax(rotatedRightPoints, axis=0)
  leftAdjCenter = (leftAdjMin + leftAdjMax) * 0.5
  rightAdjCenter = (rightAdjMin + rightAdjMax) * 0.5

  print(f"left_min = {leftAdjMin}")
  print(f"left_max = {leftAdjMax}")
  print(f"left_center = {leftAdjCenter}")
  print(f"right_min = {rightAdjMin}")
  print(f"right_max = {rightAdjMax}")
  print(f"right_center = {rightAdjCenter}")


  f.write("const uint16_t channelPointOffsets[] = {" + ",".join(channelOffsets[x] for x in channelSwizzle) + "};\n")
  f.write("const uint8_t channelLengths[] = {" + ",".join(channelLengths[x] for x in channelSwizzle) + "};\n")
  f.write("const uint8_t channelEyeIndices[] = {" + ",".join(channelEyeIndices[x] for x in channelSwizzle) + "};\n")

  f.write(f"const vec2 boundsMin = vec2({leftAdjMin[0]}f, {leftAdjMin[1]}f);\n")
  f.write(f"const vec2 boundsMax = vec2({leftAdjMax[0]}f, {leftAdjMax[1]}f);\n")
  f.write(f"const vec2 boundsCenter = vec2({leftAdjCenter[0]}f, {leftAdjCenter[1]}f);\n")

