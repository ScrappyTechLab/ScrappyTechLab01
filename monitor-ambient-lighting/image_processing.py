from PIL import Image
import cv2
import numpy as np
import os

HORIZONTAL_PIXEL_COUNT = 20
VERTICAL_PIXEL_COUNT = 20
FRACTION_OF_PIXELS_FROM_EDGE = 0.05 # based on the overall image resolution, we need to go from the edges towards the center only to a certain degree
NUMBER_OF_BOXES_HORIZONTAL = 40
NUMBER_OF_BOXES_VERTICAL = 5
REVERSE_LIGHT_STRIP_DIRECTION = True
LED_BRIGHTNESS_FACTOR = 15   # 1 is the max brightness. this value divides the actual RGB colour value
DISPLAY_AVERAGED_OUT_IMAGE = True


def retrieveImageFromFile(filePath):
    im = Image.open(filePath) # Can be many different formats.
    pix = im.load()
    # print (im.size)
    # print (pix[0,0])
    width = im.size[0]
    height = im.size[1]
    return width, height, pix


def processImage(width, height, pix):

    '''
    given the args, convert the image pixel data into an aggregate level image data
    this image can then be shown in parallel
    return the aggregated or coarse level colour information

    aggregation is done by averaging out the RGB colour informtion of all the pixels present within a single box
    '''

    depth_to_go_to_from_top_and_bottom = int(width * FRACTION_OF_PIXELS_FROM_EDGE)
    depth_to_go_to_from_sides = int(height * FRACTION_OF_PIXELS_FROM_EDGE)

    generated_image = np.zeros((height, width, 3), np.uint8)
    output_pixel_colours = np.zeros((60, 3), np.uint8)
    output_pixel_colours_for_lights = np.zeros((60, 3), np.uint8)

    # top of the image
    number_of_image_pixels_per_box_width = int(width / NUMBER_OF_BOXES_HORIZONTAL)
    reverseBoxIndex = NUMBER_OF_BOXES_HORIZONTAL - 1
    for boxIndex in range (0, NUMBER_OF_BOXES_HORIZONTAL):
        averaged_out_colour = [0, 0, 0]
        number_of_image_pixels_count = 0
        for i in range (boxIndex * number_of_image_pixels_per_box_width, (boxIndex + 1) * number_of_image_pixels_per_box_width):
            for j in range (0, depth_to_go_to_from_top_and_bottom):
                averaged_out_colour[0] += pix[i, j][0]
                averaged_out_colour[1] += pix[i, j][1]
                averaged_out_colour[2] += pix[i, j][2]
                number_of_image_pixels_count +=1
        averaged_out_colour[0] = averaged_out_colour[0] / number_of_image_pixels_count
        averaged_out_colour[1] = averaged_out_colour[1] / number_of_image_pixels_count
        averaged_out_colour[2] = averaged_out_colour[2] / number_of_image_pixels_count

        # print(averaged_out_colour)

        if DISPLAY_AVERAGED_OUT_IMAGE:
            for i in range (boxIndex * number_of_image_pixels_per_box_width, (boxIndex + 1) * number_of_image_pixels_per_box_width):
                for j in range (0, depth_to_go_to_from_top_and_bottom):
                    generated_image[j][i][0] = averaged_out_colour[2]
                    generated_image[j][i][1] = averaged_out_colour[1]
                    generated_image[j][i][2] = averaged_out_colour[0]

        #
        # output_pixel_colours[boxIndex][0] = averaged_out_colour[2]
        # output_pixel_colours[boxIndex][1] = averaged_out_colour[1]
        # output_pixel_colours[boxIndex][2] = averaged_out_colour[0]

        '''
        51 is to reduce from 255 colours to max of 5. this is to have a good representation of the colour output
        255 / 5 = 51. so for every 51 levels, one level can go up in the output
        for opencv, the sequence apparently is not RGB, but the reverse. so maintaining a separate output for the light strip
        '''
        if REVERSE_LIGHT_STRIP_DIRECTION:
            output_pixel_colours_for_lights_index = reverseBoxIndex
        else:
            output_pixel_colours_for_lights_index = boxIndex
        output_pixel_colours_for_lights[output_pixel_colours_for_lights_index][0] = averaged_out_colour[0] / LED_BRIGHTNESS_FACTOR
        output_pixel_colours_for_lights[output_pixel_colours_for_lights_index][1] = averaged_out_colour[1] / LED_BRIGHTNESS_FACTOR
        output_pixel_colours_for_lights[output_pixel_colours_for_lights_index][2] = averaged_out_colour[2] / LED_BRIGHTNESS_FACTOR

        reverseBoxIndex -=1

    if DISPLAY_AVERAGED_OUT_IMAGE:
        cv2.imshow("averaged_out_image", generated_image)
    cv2.waitKey(1)
    # print(output_pixel_colours)
    return output_pixel_colours_for_lights



def processVideoFrame(cap):
    return processImage(cap.shape[0], cap.shape[1], cap)


def processImageFromFile():
    width, height, pix = retrieveImageFromFile('Colorful-Curved-Rainbow-Pattern.jpg')
    return processImage(width, height, pix)


def initializeVideo(filePath):
    '''
    initializes a video stream of the file path passed in
    '''
    global video
    video = cv2.VideoCapture(os.path.join(filePath))
    cv2.namedWindow("original_video", cv2.WINDOW_NORMAL)


def processOneFrameFromVideo():
    '''
    this needs to be caled after initializeVideo
    this will process one frame  of the video stream, and then return the colours information
    '''

    if video.isOpened():

        # Read video capture
        ret, frame = video.read()

        # Display each frame
        cv2.imshow("original_video", frame)

        frame = cv2.resize(frame, (480, 340))
        return processVideoFrame(frame)

    else:
        # Release capture object
        video.release()
        # Exit and distroy all windows
        cv2.destroyAllWindows()
        return None



if __name__ == "__main__":
    processImage()



