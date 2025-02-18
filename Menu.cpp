#include <iostream>
#include <cstdlib> // for system("cls")
#include <string>
#include <vector>
#include <windows.h> // WinAPI for browsing directories
#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>

class ImageProcessor {
public:
    cv::Mat image;

    bool loadImage(const std::string& path) {
        image = cv::imread(path);
        if (image.empty()) {
            std::cout << "Failed to load image!" << std::endl;
            return false;
        }
        std::cout << "Image loaded successfully." << std::endl;
        resizeImageToHeight(500);
        return true;
    }

    void resizeImageToHeight(int targetHeight) {
        if (image.empty()) {
            std::cout << "No image to resize." << std::endl;
            return;
        }

        int originalWidth = image.cols;
        int originalHeight = image.rows;

        double scaleFactor = static_cast<double>(targetHeight) / originalHeight;
        int newWidth = static_cast<int>(originalWidth * scaleFactor);

        cv::resize(image, image, cv::Size(newWidth, targetHeight));
        std::cout << "Image resized to height " << targetHeight << " px." << std::endl;
    }

    void saveImage(const std::string& path) {
        if (image.empty()) {
            std::cout << "No image to save!" << std::endl;
            return;
        }
        cv::imwrite(path, image);
        std::cout << "Image saved as " << path << std::endl;
    }

    void detectEdges() {
        if (image.empty()) {
            std::cout << "No image to process!" << std::endl;
            return;
        }

        // Bilateral filter – preserves edges while reducing noise
        cv::Mat filtered;
        cv::bilateralFilter(image, filtered, 9, 75, 75);

        // Gaussian blur
        cv::Mat blurred;
        cv::GaussianBlur(filtered, blurred, cv::Size(9, 9), 1.4);

        // Convert to grayscale
        cv::Mat gray;
        cv::cvtColor(blurred, gray, cv::COLOR_BGR2GRAY);

        // Canny edge detection
        cv::Mat edges;
        cv::Canny(gray, edges, 50, 150);

        // Contrast enhancement
        cv::Mat enhancedEdges;
        cv::convertScaleAbs(edges, enhancedEdges, 2, 0);

        // Show results
        cv::imshow("Edges (Canny)", enhancedEdges);
        cv::waitKey(0);
    }

    void highlightPeople() {
        if (image.empty()) {
            std::cout << "No image to process!" << std::endl;
            return;
        }

        cv::Mat filtered;
        cv::medianBlur(image, filtered, 5);
        cv::imshow("After Median Filtering", filtered);
        cv::waitKey(0);

        cv::Mat gray;
        cv::cvtColor(filtered, gray, cv::COLOR_BGR2GRAY);

        // Apply watershed
        cv::Mat result = applyWatershed(gray, image);

        // Display result
        cv::imshow("Highlighted People (Watershed)", result);
        cv::waitKey(0);
    }

    cv::Mat applyWatershed(const cv::Mat& gray, const cv::Mat& originalImage) {
        // Show original grayscale image
        cv::imshow("1. Original Image (Grayscale)", gray);
        cv::waitKey(0);

        // Otsu's binarization
        cv::Mat binary;
        cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY + cv::THRESH_OTSU);
        cv::imshow("2. Otsu's Binarization", binary);
        cv::waitKey(0);

        // Morphological operations to improve binarization
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
        cv::morphologyEx(binary, binary, cv::MORPH_OPEN, kernel, cv::Point(-1, -1), 2);
        cv::imshow("3. After OPEN operation", binary);
        cv::waitKey(0);

        cv::morphologyEx(binary, binary, cv::MORPH_CLOSE, kernel, cv::Point(-1, -1), 2);
        cv::imshow("4. After CLOSE operation", binary);
        cv::waitKey(0);

        // Erosion to shrink objects
        cv::erode(binary, binary, kernel, cv::Point(-1, -1), 1);
        cv::imshow("5. After Erosion", binary);
        cv::waitKey(0);

        // Find sure background markers
        cv::Mat sure_bg;
        cv::dilate(binary, sure_bg, kernel, cv::Point(-1, -1), 3);
        cv::imshow("6. Sure Background (sure_bg)", sure_bg);
        cv::waitKey(0);

        cv::Mat dist_transform;
        cv::distanceTransform(binary, dist_transform, cv::DIST_L2, 5);
        cv::normalize(dist_transform, dist_transform, 0, 1.0, cv::NORM_MINMAX);
        cv::imshow("7. Distance Transform", dist_transform);
        cv::waitKey(0);

        // Find sure foreground markers
        cv::Mat sure_fg;
        double maxVal;
        cv::minMaxLoc(dist_transform, nullptr, &maxVal);
        cv::threshold(dist_transform, sure_fg, 0.5 * maxVal, 255, cv::THRESH_BINARY);
        sure_fg.convertTo(sure_fg, CV_8U);
        cv::imshow("8. Sure Foreground (sure_fg)", sure_fg);
        cv::waitKey(0);

        // Subtract sure_fg from sure_bg to get unknown regions
        cv::Mat unknown;
        cv::subtract(sure_bg, sure_fg, unknown);
        cv::imshow("9. Unknown Regions", unknown);
        cv::waitKey(0);

        // Find markers
        cv::Mat markers;
        cv::connectedComponents(sure_fg, markers);
        markers += 1;
        markers.setTo(0, unknown == 255);

        // Apply watershed on the copy of the image
        cv::Mat result = originalImage.clone();
        cv::watershed(result, markers);

        // Display markers after watershed
        cv::Mat markersAfterWatershed = markers.clone();
        markersAfterWatershed.convertTo(markersAfterWatershed, CV_8U);
        cv::imshow("11. Markers after Watershed", markersAfterWatershed * 50);
        cv::waitKey(0);

        // Create mask for non-background regions
        cv::Mat mask = cv::Mat::zeros(markers.size(), CV_8U);
        for (int i = 0; i < markers.rows; i++) {
            for (int j = 0; j < markers.cols; j++) {
                if (markers.at<int>(i, j) > 1) {
                    mask.at<uchar>(i, j) = 255;
                }
            }
        }

        // Find contours on mask
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        // Draw rectangles and labels
        for (size_t i = 0; i < contours.size(); i++) {
            cv::Rect boundingBox = cv::boundingRect(contours[i]);

            if (boundingBox.area() > 300) {
                cv::rectangle(result, boundingBox, cv::Scalar(0, 255, 0), 2);
                std::string label = "Person " + std::to_string(i + 1);
                cv::putText(result, label, cv::Point(boundingBox.x, boundingBox.y + boundingBox.height + 20),
                    cv::FONT_HERSHEY_SIMPLEX, 0.2, cv::Scalar(0, 255, 0), 2);
            }
        }

        // Display final result
        cv::imshow("12. Final Result", result);
        cv::waitKey(0);

        return result;
    }

    void detectEdgesAndDetectPersons() {
        if (image.empty()) {
            std::cout << "No image to process!" << std::endl;
            return;
        }

        // Bilateral filter – preserves edges while reducing noise
        cv::Mat filtered;
        cv::bilateralFilter(image, filtered, 9, 75, 75);

        // Gaussian blur
        cv::Mat blurred;
        cv::GaussianBlur(filtered, blurred, cv::Size(11, 11), 1.4);

        // Convert to grayscale
        cv::Mat gray;
        cv::cvtColor(blurred, gray, cv::COLOR_BGR2GRAY);

        // Canny edge detection
        cv::Mat edges;
        cv::Canny(gray, edges, 40, 160);

        // Closing operation
        cv::Mat closedEdges;
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
        cv::morphologyEx(edges, closedEdges, cv::MORPH_CLOSE, kernel);

        // Contrast enhancement
        cv::Mat enhancedEdges;
        cv::convertScaleAbs(closedEdges, enhancedEdges, 2, 0);

        // Find contours
        std::vector<std::vector<cv::Point>> contours;
        std::vector<cv::Vec4i> hierarchy;
        cv::findContours(enhancedEdges, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        // Filter valid contours
        std::vector<std::vector<cv::Point>> validContours;
        std::vector<std::vector<cv::Point>> removedContours;

        double minContourLength = 150;
        double maxContourLength = 1000;

        for (const auto& contour : contours) {
            double length = cv::arcLength(contour, true);
            if (length >= minContourLength && length <= maxContourLength) {
                validContours.push_back(contour);
            }
            else {
                removedContours.push_back(contour);
            }
        }

        // Drawing contours
        cv::Mat result = image.clone();
        cv::Mat removedContoursImage = cv::Mat::zeros(image.size(), CV_8UC3);

        for (size_t i = 0; i < validContours.size(); i++) {
            cv::Rect boundingBox = cv::boundingRect(validContours[i]);

            if (boundingBox.area() > 100) {
                cv::rectangle(result, boundingBox, cv::Scalar(0, 255, 0), 2);
                std::string label = "Person " + std::to_string(i + 1);
                cv::putText(result, label, cv::Point(boundingBox.x, boundingBox.y + boundingBox.height + 20),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 2);
            }
        }

        cv::Mat Contours = image.clone();
        cv::drawContours(Contours, removedContours, -1, cv::Scalar(0, 0, 255), 2);
        cv::drawContours(Contours, validContours, -1, cv::Scalar(0, 255, 0), 2);
        
        // Display results
        cv::imshow("Detected People", result);
        cv::imshow("Contours", Contours);

        cv::imwrite("C:/C++/Projekt_openCV/Menu/Menu/Results/result.jpg", result);
        cv::imwrite("C:/C++/Projekt_openCV/Menu/Menu/Results/Contours.jpg", Contours);
        cv::waitKey(0);
        cv::destroyAllWindows();
    }
};

class Menu {
public:
    void displayMenu() {
        std::string path;
        ImageProcessor processor;

        for (;;) {
            std::cout << "\n===== MENU =====" << std::endl;
            std::cout << "1. Load image" << std::endl;
            std::cout << "2. Save" << std::endl;
            std::cout << "3. Detect edges" << std::endl;
            std::cout << "4. Highlight people" << std::endl;
            std::cout << "5. Detect edges and highlight people" << std::endl;
            std::cout << "9. Exit" << std::endl;
            std::cout << "Choose an option: ";

            int choice;
            std::cin >> choice;

            // Clear console
            system("cls");

            switch (choice) {
            case 1: {
                std::cout << "Enter the directory path: ";
                std::cin >> path;

                std::vector<std::string> imageFiles;
                int index = 1;

                // Search directory for image files
                WIN32_FIND_DATAA findFileData;
                HANDLE hFind = FindFirstFileA((path + "\\*").c_str(), &findFileData);

                if (hFind != INVALID_HANDLE_VALUE) {
                    do {
                        std::string filename = findFileData.cFileName;
                        std::string extension = filename.substr(filename.find_last_of(".") + 1);
                        if (extension == "jpg" || extension == "png" || extension == "bmp" || extension == "jpeg" || extension == "webp") {
                            imageFiles.push_back(path + "\\" + filename);
                            std::cout << index << ". " << filename << std::endl;
                            index++;
                        }
                    } while (FindNextFileA(hFind, &findFileData) != 0);
                    FindClose(hFind);
                }
                else {
                    std::cout << "Unable to open directory!" << std::endl;
                    break;
                }

                if (imageFiles.empty()) {
                    std::cout << "No image files found in the specified location." << std::endl;
                    break;
                }

                std::cout << "Select image (1-" << imageFiles.size() << "): ";
                int imageChoice;
                std::cin >> imageChoice;

                if (imageChoice < 1 || imageChoice > imageFiles.size()) {
                    std::cout << "Invalid selection." << std::endl;
                    break;
                }

                if (processor.loadImage(imageFiles[imageChoice - 1])) {
                    std::cout << "Selected file: " << imageFiles[imageChoice - 1] << std::endl;
                }
                break;
            }
            case 2: {
                std::string savePath;
                std::cout << "Enter the path to save image: ";
                std::cin >> savePath;
                processor.saveImage(savePath);
                break;
            }
            case 3:
                processor.detectEdges();
                break;
            case 4:
                processor.highlightPeople();
                break;
            case 5:
                processor.detectEdgesAndDetectPersons();
                break;
            case 9:
                return;
            default:
                std::cout << "Invalid choice! Try again." << std::endl;
            }
        }
    }
};

int main() {
    Menu menu;
    menu.displayMenu();
    return 0;
}