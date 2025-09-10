#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/quality.hpp>
#include <random>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cxxopts.hpp> 

void addSaltPepperNoise(cv::Mat& img, double density) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);

    double saltProb = density / 2.0;

    for (int row = 0; row < img.rows; ++row) {
        for (int col = 0; col < img.cols; ++col) {
            double r = dis(gen);
            uchar& pixel = img.at<uchar>(row, col);
            if (r < saltProb) {
                pixel = 0; // pepper
            } else if (r < density) {
                pixel = 255; // salt
            }
        }
    }
}

cv::Mat createTriangularKernel(int size) {
    cv::Mat kernel = cv::Mat::zeros(size, size, CV_32F);
    int center = size / 2;
    float sum = 0.0f;

    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            float val = static_cast<float>((center + 1) - std::max(std::abs(i - center), std::abs(j - center)));
            kernel.at<float>(i, j) = val;
            sum += val;
        }
    }

    if (sum > 0) {
        kernel /= sum; // normalise
    }
    return kernel;
}

double computePSNR(const cv::Mat& img1, const cv::Mat& img2) {
    return cv::PSNR(img1, img2);
}

double computeSSIM(const cv::Mat& img1, const cv::Mat& img2) {
    cv::Ptr<cv::quality::QualitySSIM> ssim = cv::quality::QualitySSIM::create(img2);
    cv::Scalar score = ssim->compute(img1);
    return score[0];
}

void applySobelAndSave(const cv::Mat& img, const std::string& basePath, const std::string& label) {
    cv::Mat sobelH, sobelHAbs;
    cv::Sobel(img, sobelH, CV_16S, 1, 0);
    cv::convertScaleAbs(sobelH, sobelHAbs);
    cv::imwrite(basePath + "_horizontal.png", sobelHAbs);

    cv::Mat sobelV, sobelVAbs;
    cv::Sobel(img, sobelV, CV_16S, 0, 1);
    cv::convertScaleAbs(sobelV, sobelVAbs);
    cv::imwrite(basePath + "_vertical.png", sobelVAbs);

    // convert to float for magnitude calculation
    cv::Mat sobelH_f, sobelV_f;
    sobelH.convertTo(sobelH_f, CV_32F);
    sobelV.convertTo(sobelV_f, CV_32F);
    
    cv::Mat magnitude;
    cv::magnitude(sobelH_f, sobelV_f, magnitude);
    cv::convertScaleAbs(magnitude, magnitude);
    cv::imwrite(basePath + "_magnitude.png", magnitude);

    cv::Scalar meanVal = cv::mean(magnitude);
    std::cout << "Sobel magnitude mean for " << label << ": " << meanVal[0] << std::endl;
}

void processImage(const std::string& path, const std::string& imgName, double noiseDensity, const std::string& outputDir) {
    cv::Mat original = cv::imread(path, cv::IMREAD_GRAYSCALE);
    if (original.empty()) {
        std::cerr << "Failed to load image: " << path << std::endl;
        return;
    }

    auto outputPath = [&](const std::string& suffix) { return outputDir + "/" + imgName + "_" + suffix; };

    cv::imwrite(outputPath("original") + ".png", original);

    cv::Mat noisy = original.clone();
    addSaltPepperNoise(noisy, noiseDensity);
    cv::imwrite(outputPath("noisy") + ".png", noisy);

    std::cout << "\nMetrics for " << imgName << ":" << std::endl;
    std::cout << "Noisy PSNR: " << computePSNR(original, noisy) << std::endl;
    std::cout << "Noisy SSIM: " << computeSSIM(original, noisy) << std::endl;

    std::vector<int> sizes = {3, 5, 7};
    std::vector<std::tuple<std::string, int, cv::Mat>> filteredImages;

    for (int size : sizes) {
        // mean filter
        cv::Mat meanFiltered;
        cv::blur(noisy, meanFiltered, cv::Size(size, size));
        cv::imwrite(outputPath("mean_" + std::to_string(size)) + ".png", meanFiltered);
        std::cout << "Mean " << size << "x" << size << " PSNR: " << computePSNR(original, meanFiltered) << std::endl;
        std::cout << "Mean " << size << "x" << size << " SSIM: " << computeSSIM(original, meanFiltered) << std::endl;
        filteredImages.emplace_back("mean", size, meanFiltered);

        // median filter
        cv::Mat medianFiltered;
        cv::medianBlur(noisy, medianFiltered, size);
        cv::imwrite(outputPath("median_" + std::to_string(size)) + ".png", medianFiltered);
        std::cout << "Median " << size << "x" << size << " PSNR: " << computePSNR(original, medianFiltered) << std::endl;
        std::cout << "Median " << size << "x" << size << " SSIM: " << computeSSIM(original, medianFiltered) << std::endl;
        filteredImages.emplace_back("median", size, medianFiltered);

        // triangular filter
        cv::Mat kernel = createTriangularKernel(size);
        cv::Mat triangularFiltered;
        cv::filter2D(noisy, triangularFiltered, -1, kernel);
        cv::imwrite(outputPath("triangular_" + std::to_string(size)) + ".png", triangularFiltered);
        std::cout << "Triangular " << size << "x" << size << " PSNR: " << computePSNR(original, triangularFiltered) << std::endl;
        std::cout << "Triangular " << size << "x" << size << " SSIM: " << computeSSIM(original, triangularFiltered) << std::endl;
        filteredImages.emplace_back("triangular", size, triangularFiltered);
    }

    applySobelAndSave(original, outputPath("sobel_original"), "original");
    applySobelAndSave(noisy, outputPath("sobel_noisy"), "noisy");

    for (const auto& [filterType, size, filtered] : filteredImages) {
        std::string prefix = "sobel_" + filterType + "_" + std::to_string(size);
        std::string label = filterType + " " + std::to_string(size) + "x" + std::to_string(size);
        applySobelAndSave(filtered, outputPath(prefix), label);
    }
}

int main(int argc, char** argv) {
    cxxopts::Options options("kyubey", "A command-line instrument for quantitative analysis of grayscale images through the programmatic application of stochastic noise, spatial filtering, and gradient-based edge detection.");
    options.add_options()
        ("1,image1", "Path to the first grayscale input image artifact.", cxxopts::value<std::string>())
        ("2,image2", "Path to the second grayscale input image artifact.", cxxopts::value<std::string>())
        ("d,density", "Specifies the stochastic noise density. Accepts a value in the range [0.0, 1.0].", cxxopts::value<double>()->default_value("0.10"))
        ("o,output-dir", "Specifies the directory for output artifacts.", cxxopts::value<std::string>()->default_value("output"))
        ("h,help", "Prints this usage summary.");

    auto result = options.parse(argc, argv);

    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        return 0;
    }

    if (!result.count("image1") || !result.count("image2")) {
        std::cerr << "Error: Both --image1 and --image2 are required." << std::endl;
        std::cout << options.help() << std::endl;
        return 1;
    }

    std::string image1 = result["image1"].as<std::string>();
    std::string image2 = result["image2"].as<std::string>();
    double noiseDensity = result["density"].as<double>();
    std::string outputDir = result["output-dir"].as<std::string>();

    std::filesystem::create_directories(outputDir);

    processImage(image1, "image1", noiseDensity, outputDir);
    processImage(image2, "image2", noiseDensity, outputDir);

    return 0;
}
