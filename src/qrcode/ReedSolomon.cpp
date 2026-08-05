#include "ReedSolomon.hpp"

#include "GaloisField256.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace qrcode {
namespace {

class Polynomial {
public:
    explicit Polynomial(std::vector<std::uint8_t> coefficients)
        : coefficients_(std::move(coefficients)) {
        normalize();
    }

    [[nodiscard]] static Polynomial zero() {
        return Polynomial{{0}};
    }

    [[nodiscard]] static Polynomial one() {
        return Polynomial{{1}};
    }

    [[nodiscard]] static Polynomial monomial(int degree,
                                             std::uint8_t coefficient) {
        if (degree < 0 || coefficient == 0) {
            return zero();
        }
        std::vector<std::uint8_t> coefficients(
            static_cast<std::size_t>(degree) + 1, 0);
        coefficients.front() = coefficient;
        return Polynomial{std::move(coefficients)};
    }

    [[nodiscard]] int degree() const {
        return static_cast<int>(coefficients_.size()) - 1;
    }

    [[nodiscard]] bool isZero() const {
        return coefficients_.front() == 0;
    }

    [[nodiscard]] std::uint8_t coefficient(int degree) const {
        return coefficients_.at(coefficients_.size() - 1 - degree);
    }

    [[nodiscard]] std::uint8_t evaluateAt(std::uint8_t value) const {
        if (value == 0) {
            return coefficient(0);
        }

        std::uint8_t result = 0;
        for (const auto coefficient : coefficients_) {
            result = GaloisField256::add(
                GaloisField256::multiply(result, value), coefficient);
        }
        return result;
    }

    [[nodiscard]] Polynomial add(const Polynomial& other) const {
        const auto* smaller = &coefficients_;
        const auto* larger = &other.coefficients_;
        if (smaller->size() > larger->size()) {
            std::swap(smaller, larger);
        }

        std::vector<std::uint8_t> sum = *larger;
        const auto offset = larger->size() - smaller->size();
        for (std::size_t index = 0; index < smaller->size(); ++index) {
            sum.at(index + offset) =
                GaloisField256::add(sum.at(index + offset), smaller->at(index));
        }
        return Polynomial{std::move(sum)};
    }

    [[nodiscard]] Polynomial multiply(const Polynomial& other) const {
        if (isZero() || other.isZero()) {
            return zero();
        }

        std::vector<std::uint8_t> product(
            coefficients_.size() + other.coefficients_.size() - 1, 0);
        for (std::size_t left = 0; left < coefficients_.size(); ++left) {
            for (std::size_t right = 0;
                 right < other.coefficients_.size(); ++right) {
                const auto term = GaloisField256::multiply(
                    coefficients_.at(left), other.coefficients_.at(right));
                product.at(left + right) =
                    GaloisField256::add(product.at(left + right), term);
            }
        }
        return Polynomial{std::move(product)};
    }

    [[nodiscard]] Polynomial multiply(std::uint8_t scalar) const {
        if (scalar == 0) {
            return zero();
        }
        if (scalar == 1) {
            return *this;
        }

        std::vector<std::uint8_t> product(coefficients_.size());
        for (std::size_t index = 0; index < coefficients_.size(); ++index) {
            product.at(index) =
                GaloisField256::multiply(coefficients_.at(index), scalar);
        }
        return Polynomial{std::move(product)};
    }

    [[nodiscard]] Polynomial multiplyByMonomial(
        int degree, std::uint8_t coefficient) const {
        if (degree < 0 || coefficient == 0) {
            return zero();
        }

        std::vector<std::uint8_t> product(
            coefficients_.size() + static_cast<std::size_t>(degree), 0);
        for (std::size_t index = 0; index < coefficients_.size(); ++index) {
            product.at(index) =
                GaloisField256::multiply(coefficients_.at(index), coefficient);
        }
        return Polynomial{std::move(product)};
    }

private:
    void normalize() {
        const auto firstNonzero =
            std::find_if(coefficients_.begin(), coefficients_.end(),
                         [](std::uint8_t value) { return value != 0; });
        if (firstNonzero == coefficients_.end()) {
            coefficients_ = {0};
        } else if (firstNonzero != coefficients_.begin()) {
            coefficients_.erase(coefficients_.begin(), firstNonzero);
        }
    }

    std::vector<std::uint8_t> coefficients_;
};

struct EuclideanResult {
    Polynomial errorLocator;
    Polynomial errorEvaluator;
};

std::optional<EuclideanResult> runEuclideanAlgorithm(
    const Polynomial& syndrome, int errorCorrectionCodewords) {
    auto previousRemainder =
        Polynomial::monomial(errorCorrectionCodewords, 1);
    auto remainder = syndrome;
    if (previousRemainder.degree() < remainder.degree()) {
        std::swap(previousRemainder, remainder);
    }

    auto previousAuxiliary = Polynomial::zero();
    auto auxiliary = Polynomial::one();

    while (remainder.degree() >= errorCorrectionCodewords / 2) {
        const auto previousPreviousRemainder = previousRemainder;
        const auto previousPreviousAuxiliary = previousAuxiliary;
        previousRemainder = remainder;
        previousAuxiliary = auxiliary;

        if (previousRemainder.isZero()) {
            return std::nullopt;
        }

        remainder = previousPreviousRemainder;
        auto quotient = Polynomial::zero();
        const auto denominatorLeading =
            previousRemainder.coefficient(previousRemainder.degree());
        const auto denominatorInverse =
            GaloisField256::inverse(denominatorLeading);
        if (!denominatorInverse.has_value()) {
            return std::nullopt;
        }

        while (!remainder.isZero() &&
               remainder.degree() >= previousRemainder.degree()) {
            const int degreeDifference =
                remainder.degree() - previousRemainder.degree();
            const auto scale = GaloisField256::multiply(
                remainder.coefficient(remainder.degree()),
                *denominatorInverse);
            quotient = quotient.add(
                Polynomial::monomial(degreeDifference, scale));
            remainder = remainder.add(previousRemainder.multiplyByMonomial(
                degreeDifference, scale));
        }

        auxiliary =
            quotient.multiply(previousAuxiliary).add(previousPreviousAuxiliary);
    }

    const auto constant = auxiliary.coefficient(0);
    const auto inverse = GaloisField256::inverse(constant);
    if (!inverse.has_value()) {
        return std::nullopt;
    }

    return EuclideanResult{
        .errorLocator = auxiliary.multiply(*inverse),
        .errorEvaluator = remainder.multiply(*inverse),
    };
}

std::optional<std::vector<std::uint8_t>> findErrorLocations(
    const Polynomial& errorLocator) {
    const int numberOfErrors = errorLocator.degree();
    std::vector<std::uint8_t> locations;
    locations.reserve(numberOfErrors);

    for (int value = 1; value < 256 &&
                        static_cast<int>(locations.size()) < numberOfErrors;
         ++value) {
        if (errorLocator.evaluateAt(static_cast<std::uint8_t>(value)) == 0) {
            const auto inverse =
                GaloisField256::inverse(static_cast<std::uint8_t>(value));
            if (!inverse.has_value()) {
                return std::nullopt;
            }
            locations.push_back(*inverse);
        }
    }

    if (static_cast<int>(locations.size()) != numberOfErrors) {
        return std::nullopt;
    }
    return locations;
}

std::optional<std::vector<std::uint8_t>> findErrorMagnitudes(
    const Polynomial& errorEvaluator,
    std::span<const std::uint8_t> errorLocations) {
    std::vector<std::uint8_t> magnitudes(errorLocations.size());

    for (std::size_t index = 0; index < errorLocations.size(); ++index) {
        const auto locationInverse =
            GaloisField256::inverse(errorLocations[index]);
        if (!locationInverse.has_value()) {
            return std::nullopt;
        }

        std::uint8_t denominator = 1;
        for (std::size_t other = 0; other < errorLocations.size(); ++other) {
            if (index == other) {
                continue;
            }
            const auto term = GaloisField256::multiply(
                errorLocations[other], *locationInverse);
            denominator = GaloisField256::multiply(
                denominator, GaloisField256::add(1, term));
        }

        const auto denominatorInverse =
            GaloisField256::inverse(denominator);
        if (!denominatorInverse.has_value()) {
            return std::nullopt;
        }
        magnitudes.at(index) = GaloisField256::multiply(
            errorEvaluator.evaluateAt(*locationInverse),
            *denominatorInverse);
    }

    return magnitudes;
}

std::vector<std::uint8_t> calculateSyndromes(
    std::span<const std::uint8_t> codewords, int errorCorrectionCodewords) {
    const Polynomial received{
        std::vector<std::uint8_t>{codewords.begin(), codewords.end()}};
    std::vector<std::uint8_t> syndromes(errorCorrectionCodewords);

    for (int index = 0; index < errorCorrectionCodewords; ++index) {
        syndromes.at(errorCorrectionCodewords - 1 - index) =
            received.evaluateAt(GaloisField256::exponent(index));
    }
    return syndromes;
}

bool containsOnlyZeroes(std::span<const std::uint8_t> values) {
    return std::ranges::all_of(values,
                               [](std::uint8_t value) { return value == 0; });
}

}  // namespace

std::expected<ReedSolomonResult, ReedSolomonError> ReedSolomon::correct(
    std::span<const std::uint8_t> codewords,
    int errorCorrectionCodewords) {
    if (codewords.empty() || codewords.size() > 255) {
        return std::unexpected(ReedSolomonError::invalidCodewordCount);
    }
    if (errorCorrectionCodewords <= 0 ||
        errorCorrectionCodewords >= static_cast<int>(codewords.size())) {
        return std::unexpected(
            ReedSolomonError::invalidErrorCorrectionCount);
    }

    auto corrected = std::vector<std::uint8_t>{codewords.begin(),
                                               codewords.end()};
    auto syndromes =
        calculateSyndromes(corrected, errorCorrectionCodewords);
    if (containsOnlyZeroes(syndromes)) {
        return ReedSolomonResult{
            .codewords = std::move(corrected),
            .correctedErrors = 0,
        };
    }

    const Polynomial syndromePolynomial{std::move(syndromes)};
    const auto euclidean =
        runEuclideanAlgorithm(syndromePolynomial, errorCorrectionCodewords);
    if (!euclidean.has_value()) {
        return std::unexpected(ReedSolomonError::uncorrectable);
    }

    const auto errorLocations =
        findErrorLocations(euclidean->errorLocator);
    if (!errorLocations.has_value() ||
        static_cast<int>(errorLocations->size()) >
            errorCorrectionCodewords / 2) {
        return std::unexpected(ReedSolomonError::uncorrectable);
    }

    const auto errorMagnitudes =
        findErrorMagnitudes(euclidean->errorEvaluator, *errorLocations);
    if (!errorMagnitudes.has_value()) {
        return std::unexpected(ReedSolomonError::uncorrectable);
    }

    for (std::size_t index = 0; index < errorLocations->size(); ++index) {
        const auto logarithm =
            GaloisField256::logarithm(errorLocations->at(index));
        if (!logarithm.has_value()) {
            return std::unexpected(ReedSolomonError::uncorrectable);
        }

        const int position =
            static_cast<int>(corrected.size()) - 1 - *logarithm;
        if (position < 0 || position >= static_cast<int>(corrected.size())) {
            return std::unexpected(ReedSolomonError::uncorrectable);
        }
        corrected.at(position) = GaloisField256::add(
            corrected.at(position), errorMagnitudes->at(index));
    }

    const auto verification =
        calculateSyndromes(corrected, errorCorrectionCodewords);
    if (!containsOnlyZeroes(verification)) {
        return std::unexpected(ReedSolomonError::uncorrectable);
    }

    return ReedSolomonResult{
        .codewords = std::move(corrected),
        .correctedErrors = static_cast<int>(errorLocations->size()),
    };
}

}  // namespace qrcode
