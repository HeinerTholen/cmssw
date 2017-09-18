#include "RecoCTPPS/PixelLocal/interface/RPixPlaneCombinatoryTracking.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include <algorithm>


#include "DataFormats/GeometryVector/interface/GlobalPoint.h"

#include "TMatrixD.h"
#include "TVectorD.h"
#include "TVector3.h"
#include "DataFormats/Math/interface/Error.h"
#include "DataFormats/Math/interface/AlgebraicROOTObjects.h"
#include "TMath.h"
#include <cmath>
#include <algorithm>

//------------------------------------------------------------------------------------------------//

RPixPlaneCombinatoryTracking::RPixPlaneCombinatoryTracking(edm::ParameterSet const& parameterSet) : 
  RPixDetTrackFinder(parameterSet){

  trackMinNumberOfPoints_ = parameterSet.getParameter<uint>("trackMinNumberOfPoints");
  verbosity_ = parameterSet.getUntrackedParameter<int> ("verbosity");
  maximumChi2OverNDF_ = parameterSet.getParameter<double>("maximumChi2OverNDF");
  maximumXLocalDistanceFromTrack_= parameterSet.getParameter<double>("maximumXLocalDistanceFromTrack");
  maximumYLocalDistanceFromTrack_= parameterSet.getParameter<double>("maximumYLocalDistanceFromTrack");
  numberOfPlanesPerPot_ = parameterSet.getParameter<int> ("numberOfPlanesPerPot"   );

  if(trackMinNumberOfPoints_<3){
    throw cms::Exception("RPixPlaneCombinatoryTracking") << "Minimum number of planes required for tracking is 3, "
                                                         << "tracking is not possible with " 
                                                         << trackMinNumberOfPoints_ << " hits";
  }
 
  
}

//------------------------------------------------------------------------------------------------//
    
RPixPlaneCombinatoryTracking::~RPixPlaneCombinatoryTracking() {
  possiblePlaneCombinations_.clear();
}

//------------------------------------------------------------------------------------------------//

void RPixPlaneCombinatoryTracking::initialize(){
 
  uint32_t numberOfCombinations = factorial(numberOfPlanesPerPot_)/
                                  (factorial(numberOfPlanesPerPot_-trackMinNumberOfPoints_)
                                   *factorial(trackMinNumberOfPoints_));
  if(verbosity_>=2) std::cout<<"Number of combinations = "<<numberOfCombinations<<std::endl;
  possiblePlaneCombinations_.reserve(numberOfCombinations);

  possiblePlaneCombinations_ = getPlaneCombinations(listOfAllPlanes_,trackMinNumberOfPoints_);

  if(verbosity_>=2) {
    for( const auto & vec : possiblePlaneCombinations_){
      for( const auto & num : vec){
        std::cout<<num<<" - ";
      }
      std::cout<<std::endl;
    }
  }

}

//------------------------------------------------------------------------------------------------//
    
//This function produces all the possible plane combinations extracting numberToExtract planes over numberOfPlanes planes
RPixPlaneCombinatoryTracking::PlaneCombinations
RPixPlaneCombinatoryTracking::getPlaneCombinations(std::vector<uint32_t> inputPlaneList, uint32_t numberToExtract)
{
    uint32_t numberOfPlanes = inputPlaneList.size();
    std::string bitmask(numberToExtract, 1); // numberToExtract leading 1's
    bitmask.resize(numberOfPlanes, 0); // numberOfPlanes-numberToExtract trailing 0's
    PlaneCombinations planeCombinations;

    // store the combination and permute bitmask
    do {
      std::vector<uint32_t> tmpPlaneList;
      for (uint32_t i = 0; i < numberOfPlanes; ++i) { // [0..numberOfPlanes-1] integers
        if (bitmask[i]) tmpPlaneList.push_back(inputPlaneList.at(i));
      }
        planeCombinations.push_back(tmpPlaneList);
    } while (std::prev_permutation(bitmask.begin(), bitmask.end()));

    return planeCombinations;
}


//------------------------------------------------------------------------------------------------//

//This function calls it self in order to get all the possible combinations of hits having a certain selected number of planes
//This function avoids to write for loops in cascade which will not allow to define arbitrarily the minimum number of planes to use
//The output is stored in a map containing the vector of points and as a key the map of the point forming this vector.
//This allows to erase the points already used for the track fit
void RPixPlaneCombinatoryTracking::getHitCombinations(
  const std::map<CTPPSPixelDetId, PointInPlaneList > &mapOfAllHits, 
  std::map<CTPPSPixelDetId, PointInPlaneList >::iterator mapIterator,
  HitReferences tmpHitPlaneMap,
  PointInPlaneList tmpHitVector,
  PointAndReferenceMap &outputMap)
{
    //At this point I selected one hit per plane
    if (mapIterator == mapOfAllHits.end())
    {
        outputMap[tmpHitPlaneMap] = tmpHitVector;
        return;
    }
    for (size_t i=0; i<mapIterator->second.size(); i++){
      HitReferences newHitPlaneMap = tmpHitPlaneMap;
      newHitPlaneMap[mapIterator->first] = i;
      PointInPlaneList newVector = tmpHitVector;
      newVector.push_back(mapIterator->second.at(i));
      std::map<CTPPSPixelDetId, PointInPlaneList >::iterator tmpMapIterator = mapIterator;
      getHitCombinations(mapOfAllHits, ++tmpMapIterator, newHitPlaneMap, newVector, outputMap);
    }
}

//------------------------------------------------------------------------------------------------//

RPixPlaneCombinatoryTracking::PointAndReferenceMap
RPixPlaneCombinatoryTracking::produceAllHitCombination(PlaneCombinations inputPlaneCombination){
  
  PointAndReferenceMap mapOfAllPoints;
  CTPPSPixelDetId tmpRpId = romanPotId_; //in order to avoid to modify the data member
  
  if(verbosity_>2) std::cout<<"Searching for all combinations..."<<std::endl;
  //Loop on all the plane combinations
  for( const auto & planeCombination : inputPlaneCombination){

    std::map<CTPPSPixelDetId, PointInPlaneList > selectedCombinationHitOnPlane;
    bool allPlaneAsHits = true;

    //Loop on all the possible combinations
    //In this loop the selectedCombinationHitOnPlane is filled 
    //and cases in which one of the selected plane is empty are skipped
    for( const auto & plane : planeCombination){
      tmpRpId.setPlane(plane);
      CTPPSPixelDetId planeDetId = tmpRpId;
      if(hitMap_.find(planeDetId) == hitMap_.end()){
        if(verbosity_>2) std::cout<<"No data on arm "<<planeDetId.arm()<<" station "<<planeDetId.station()
                                  <<" rp " <<planeDetId.rp()<<" plane "<<planeDetId.plane()<<std::endl;
        allPlaneAsHits = false;
        break;
      }
      if(selectedCombinationHitOnPlane.find(planeDetId)!=selectedCombinationHitOnPlane.end()){
           throw cms::Exception("RPixPlaneCombinatoryTracking") 
             <<"selectedCombinationHitOnPlane contains already detId "<<planeDetId<<std::endl
             <<"Error in the algorithm which created all the possible plane combinations";
      }
      selectedCombinationHitOnPlane[planeDetId] = hitMap_[planeDetId];
    }
    if(!allPlaneAsHits) continue;
    
    //I add the all the hit combinations to the full list of plane combinations
    auto mapIterator=selectedCombinationHitOnPlane.begin();
    HitReferences tmpHitPlaneMap; //empty map of plane id and hit number needed the getHitCombinations algorithm
    PointInPlaneList tmpHitVector; //empty vector of hits needed for the getHitCombinations algorithm
    getHitCombinations(selectedCombinationHitOnPlane,mapIterator,tmpHitPlaneMap,tmpHitVector,mapOfAllPoints);
    if(verbosity_>2) std::cout<<"Number of possible tracks "<<mapOfAllPoints.size()<<std::endl;

  } //end of loops on all the combinations

  return mapOfAllPoints;

}

//------------------------------------------------------------------------------------------------//

void RPixPlaneCombinatoryTracking::findTracks(){

  //The loop search for all the possible tracks starting from the one with the smallest chiSquare/NDF
  //The loop stops when the number of planes with recorded hits is less than the minimum number of planes required
  //or if the track with minimum chiSquare found has a chiSquare higher than the maximum required

  while(hitMap_.size()>=trackMinNumberOfPoints_){

    if(verbosity_>=1) std::cout<<"Number of plane with hits "<<hitMap_.size()<<std::endl;
    if(verbosity_>=2) for(const auto & plane : hitMap_) std::cout<<"\tarm "<<plane.first.arm()
                                                                 <<" station "<<plane.first.station()
                                                                 <<" rp " <<plane.first.rp()
                                                                 <<" plane "<<plane.first.plane()
                                                                 <<" : "<<plane.second.size()<<std::endl;
    
    //I create the map of all the possible combinations of a group of trackMinNumberOfPoints_ points
    //and the key keeps the reference of which planes and which hit numbers form the combination
    PointAndReferenceMap mapOfAllMinRequiredPoint;
    //I produce the map for all cominations of all hits with all trackMinNumberOfPoints_ plane combinations
    mapOfAllMinRequiredPoint =produceAllHitCombination(possiblePlaneCombinations_);

    //Fit all the possible combinations with minimum number of planes required and find the track with minimum chi2
    double theMinChiSquaredOverNDF = maximumChi2OverNDF_+1.; //in order to break the loop in case no track is found;
    HitReferences pointMapWithMinChiSquared;
    PointInPlaneList pointsWithMinChiSquared;
    CTPPSPixelLocalTrack bestTrack;

    if(verbosity_>2) std::cout<<"Number of combinations of trackMinNumberOfPoints_ planes "
                              <<mapOfAllMinRequiredPoint.size()<<std::endl;
    for(const auto & pointsAndRef : mapOfAllMinRequiredPoint){
      CTPPSPixelLocalTrack tmpTrack = fitTrack(pointsAndRef.second);
      double tmpChiSquaredOverNDF = tmpTrack.getChiSquaredOverNDF();
      if(verbosity_>=2) std::cout<<"ChiSquare of the present track "<<tmpChiSquaredOverNDF<<std::endl;
      if(!tmpTrack.isValid() || tmpChiSquaredOverNDF>maximumChi2OverNDF_ || tmpChiSquaredOverNDF==0.) continue; //validity check
      if(tmpChiSquaredOverNDF<theMinChiSquaredOverNDF){
        theMinChiSquaredOverNDF = tmpChiSquaredOverNDF;
        pointMapWithMinChiSquared = pointsAndRef.first;
        pointsWithMinChiSquared = pointsAndRef.second;
        bestTrack = tmpTrack;
      }
    }

    //The loop on the fit of all tracks is done, the track with minimum chiSquared is found
    // and it is verified that it complies with the maximumChi2OverNDF_ requirement
    if(theMinChiSquaredOverNDF > maximumChi2OverNDF_) break;

    //The list of planes not included in the minimum chiSquare track is produced.
    std::vector<uint32_t>  listOfExcludedPlanes;
    for(const auto & plane : listOfAllPlanes_){
      CTPPSPixelDetId tmpRpId = romanPotId_; //in order to avoid to modify the data member
      tmpRpId.setPlane(plane);
      CTPPSPixelDetId planeDetId = tmpRpId;
      if(pointMapWithMinChiSquared.find(planeDetId)==pointMapWithMinChiSquared.end()) 
        listOfExcludedPlanes.push_back(plane);
    }

    //I produce all the possible combinations of planes to be added to the track,
    //excluding the case of no other plane added since it has been already fitted.
    PlaneCombinations planePointedHitListCombination;
    for(uint32_t i=1; i<=listOfExcludedPlanes.size(); ++i){
      PlaneCombinations tmpPlaneCombination = getPlaneCombinations(listOfExcludedPlanes,i);
      for(const auto & combination : tmpPlaneCombination) planePointedHitListCombination.push_back(combination);
    }

    //I produce all the possible combinations of points to be added to the track
    PointAndReferenceMap mapOfAllPointWithAtLeastBestFitSelected;
    PointAndReferenceMap mapOfPointCombinationToBeAdded;
    mapOfPointCombinationToBeAdded = produceAllHitCombination(planePointedHitListCombination);
    //The found hit combination is added to the hits selected by the best fit;
    for(const auto & element : mapOfPointCombinationToBeAdded){
      HitReferences newPointMap = pointMapWithMinChiSquared; 
      PointInPlaneList newPoints = pointsWithMinChiSquared;
      for(const auto & pointRef : element.first ) newPointMap[pointRef.first] = pointRef.second; //add the new point reference
      for(const auto & point    : element.second) newPoints.push_back(point);
      mapOfAllPointWithAtLeastBestFitSelected[newPointMap]=newPoints;
    }
  
    //I fit all the possible combination of the minimum plane best fit hits plus hits from the other planes
    if(verbosity_>=1) std::cout<<"Minimum chiSquare over NDF for all the tracks "<<theMinChiSquaredOverNDF<<std::endl;

    // I look for the tracks with maximum number of points with a chiSquare over NDF smaller than maximumChi2OverNDF_
    // If more than one track fulfill the chiSquare requirement with the same number of points I choose the one with smaller chiSquare
    std::vector<PointAndReferencePair > orderedVectorOfAllPointWithAtLeastBestFitSelected =
      orderCombinationsPerNumberOrPoints(mapOfAllPointWithAtLeastBestFitSelected);
    int currentNumberOfPlanes = 0;
    theMinChiSquaredOverNDF = maximumChi2OverNDF_+1.; //in order to break the loop in case no track is found;
    bool foundTrackWithCurrentNumberOfPlanes = false;
    for(const auto & pointsAndRef : orderedVectorOfAllPointWithAtLeastBestFitSelected){
      int tmpNumberOfPlanes = pointsAndRef.second.size();
      // If a valid track has been already found with an higher number of planes the loop stops.
      if(foundTrackWithCurrentNumberOfPlanes && tmpNumberOfPlanes<currentNumberOfPlanes) break;
      CTPPSPixelLocalTrack tmpTrack = fitTrack(pointsAndRef.second);
      double tmpChiSquaredOverNDF = tmpTrack.getChiSquaredOverNDF();
      if(!tmpTrack.isValid() || tmpChiSquaredOverNDF>maximumChi2OverNDF_ || tmpChiSquaredOverNDF==0.) continue; //validity check
      if(tmpChiSquaredOverNDF<theMinChiSquaredOverNDF){
        theMinChiSquaredOverNDF = tmpChiSquaredOverNDF;
        pointMapWithMinChiSquared = pointsAndRef.first;
        pointsWithMinChiSquared = pointsAndRef.second;
        bestTrack = tmpTrack;
        currentNumberOfPlanes = tmpNumberOfPlanes;
        foundTrackWithCurrentNumberOfPlanes = true;
      }
    }

    if(verbosity_>=1) std::cout<<"The best track has " << bestTrack.getNDF()/2 + 2 <<std::endl;
   
    std::vector<uint32_t>  listOfPlaneNotUsedForFit = listOfAllPlanes_;
    //remove the hits belonging to the tracks from the full list of hits
    for(const auto & hitToErase : pointMapWithMinChiSquared){
      std::map<CTPPSPixelDetId, PointInPlaneList >::iterator hitMapElement = hitMap_.find(hitToErase.first);
      if(hitMapElement==hitMap_.end()){
           throw cms::Exception("RPixPlaneCombinatoryTracking") 
             <<"The found tracks has hit belonging to a plane which does not have hits";
      }
      std::vector<uint32_t>::iterator planeIt;
      planeIt = std::find (listOfPlaneNotUsedForFit.begin(), listOfPlaneNotUsedForFit.end(), hitToErase.first.plane());
      listOfPlaneNotUsedForFit.erase(planeIt);
      hitMapElement->second.erase(hitMapElement->second.begin()+hitToErase.second);
      //if the plane at which the hit was erased is empty it is removed from the hit map
      if(hitMapElement->second.empty()) hitMap_.erase(hitMapElement);
    }

    //search for hit on the other planes which may belong to the same track
    //even if they did not contributed to the track
    // in case of multiple hit, the closest one to the track will be considered
    //If an hit is found these will not be erased from the list of all hits
    //If not hit is found, the point on the plane intersecting the track will be saved by a CTPPSPixelFittedRecHit 
    //with the isRealHit_ flag set to false
    for(const auto & plane : listOfPlaneNotUsedForFit){
      CTPPSPixelDetId tmpPlaneId = romanPotId_; //in order to avoid to modify the data member
      tmpPlaneId.setPlane(plane);
      std::unique_ptr<CTPPSPixelLocalTrack::CTPPSPixelFittedRecHit> 
        fittedRecHit(new CTPPSPixelLocalTrack::CTPPSPixelFittedRecHit());
      math::GlobalPoint pointOnDet;
      calculatePointOnDetector(bestTrack, tmpPlaneId, pointOnDet);


      if(hitMap_.find(tmpPlaneId) != hitMap_.end()){
        //I convert the hit search window defined in local coordinated into global
        //This avoids to convert the global plane-line intersection in order not to call the the geometry 
        TVectorD maxGlobalPointDistance(0,2,maximumXLocalDistanceFromTrack_,maximumYLocalDistanceFromTrack_,0.,"END");
        TMatrixD tmpPlaneRotationMatrixMap = planeRotationMatrixMap_[tmpPlaneId];
        maxGlobalPointDistance = tmpPlaneRotationMatrixMap * maxGlobalPointDistance;
        //I avoid the Sqrt since it will not be saved
        double maximumXdistance = maxGlobalPointDistance[0]*maxGlobalPointDistance[0];
        double maximumYdistance = maxGlobalPointDistance[1]*maxGlobalPointDistance[1];
        double minimumDistance = 1. + maximumXdistance + maximumYdistance; // to be sure that the first min distance is from a real point
        for(const auto & hit : hitMap_[tmpPlaneId]){
          double xResidual = hit.globalPoint.x() - pointOnDet.x();
          double yResidual = hit.globalPoint.y() - pointOnDet.y();
          double xDistance = xResidual*xResidual;
          double yDistance = yResidual*yResidual;
          double distance = xDistance + yDistance;
          if(xDistance < maximumXdistance && yDistance < maximumYdistance && distance < minimumDistance){
            LocalPoint residuals(xResidual,yResidual,0.);
            TMatrixD globalError = hit.globalError;
            LocalPoint pulls(xResidual/std::sqrt(globalError[0][0]),yResidual/std::sqrt(globalError[1][1]),0.);
            fittedRecHit.reset(new CTPPSPixelLocalTrack::CTPPSPixelFittedRecHit(hit.recHit, pointOnDet, residuals, pulls));
            fittedRecHit->setIsRealHit(true);
          }
        }
      }
      else{
        LocalPoint fakePoint;
        LocalError fakeError;
        CTPPSPixelRecHit fakeRecHit(fakePoint,fakeError);
        fittedRecHit.reset(new CTPPSPixelLocalTrack::CTPPSPixelFittedRecHit(fakeRecHit, pointOnDet, fakePoint, fakePoint));
      }

      bestTrack.addHit(tmpPlaneId, *fittedRecHit);
  
    }

    localTrackVector_.push_back(bestTrack);

    int pointForTracking = 0;
    int pointOnTrack = 0;

    if(verbosity_>=1){
      for(const auto & planeHits : bestTrack.getHits()){
        for(const auto & fittedhit : planeHits){
          if(fittedhit.getIsUsedForFit()) ++pointForTracking;
          if(fittedhit.getIsRealHit()) ++pointOnTrack;
        }
      }
      std::cout<<"Best track has "<<pointForTracking<<" points used for the fit and "
               << pointOnTrack<<" points belonging to the track\n";
    }

  } //close of the while loop on all the hits

  return;

}

//------------------------------------------------------------------------------------------------//

CTPPSPixelLocalTrack RPixPlaneCombinatoryTracking::fitTrack(PointInPlaneList pointList){
  
  uint32_t numberOfPoints = pointList.size();
  TVectorD xyCoordinates(2*numberOfPoints);
  TMatrixD varianceMatrix(2*numberOfPoints,2*numberOfPoints);
  TMatrixD zMatrix(2*numberOfPoints,4);
  
  //The matrices and vector xyCoordinates, varianceMatrix and varianceMatrix are built from the points
  uint32_t hitCounter=0;
  for(const auto & hit : pointList){
    CLHEP::Hep3Vector globalPoint  = hit.globalPoint    ;
    xyCoordinates[hitCounter]      = globalPoint.x()    ;
    xyCoordinates[hitCounter+1]    = globalPoint.y()    ;
    zMatrix      [hitCounter][0]   = 1.                 ;
    zMatrix      [hitCounter][2]   = globalPoint.z()-z0_;
    zMatrix      [hitCounter+1][1] = 1.                 ;
    zMatrix      [hitCounter+1][3] = globalPoint.z()-z0_;

    TMatrixD globalError = hit.globalError;
    varianceMatrix[hitCounter][hitCounter]     = globalError[0][0];
    varianceMatrix[hitCounter][hitCounter+1]   = globalError[0][1];
    varianceMatrix[hitCounter+1][hitCounter]   = globalError[1][0];
    varianceMatrix[hitCounter+1][hitCounter+1] = globalError[1][1];
    
    hitCounter+=2;
  }

  //for having the real point variance matrix, varianceMatrix need to be inverted
  try{
    varianceMatrix.Invert();
  }
  catch (cms::Exception &e){
    edm::LogError("RPixPlaneCombinatoryTracking") << "Error in RPixPlaneCombinatoryTracking::fitTrack -> "
    << "Point variance matrix is singular, skipping.";
    CTPPSPixelLocalTrack badTrack;
    badTrack.setValid(false);
    return badTrack;
  }

  TMatrixD zMatrixTranspose(TMatrixD::kTransposed,zMatrix);
  TMatrixD zMatrixTransposeTimesVarianceMatrix = zMatrixTranspose *  varianceMatrix;
  TMatrixD parametersCovarianceMatrix = zMatrixTransposeTimesVarianceMatrix * zMatrix;

  //for having the real parameter covaraince matrix parametersCovarianceMatrix, need to be inverted
  try{
    parametersCovarianceMatrix.Invert();
  }
  catch (cms::Exception &e){
    edm::LogError("RPixPlaneCombinatoryTracking") << "Error in RPixPlaneCombinatoryTracking::fitTrack -> "
    << "Parameter covariance matrix is singular, skipping.";
    CTPPSPixelLocalTrack badTrack;
    badTrack.setValid(false);
    return badTrack;
  }

  // track parameters: (x0, y0, tx, ty); x = x0 + tx*(z-z0)
  TVectorD zMatrixTransposeTimesVarianceMatrixTimesXyCoordinates= zMatrixTransposeTimesVarianceMatrix * xyCoordinates;
  TVectorD parameters = parametersCovarianceMatrix*zMatrixTransposeTimesVarianceMatrixTimesXyCoordinates;
  TVectorD xyCoordinatesMinusZmatrixTimesParameters = xyCoordinates - (zMatrix * parameters);

  double chiSquare = xyCoordinatesMinusZmatrixTimesParameters 
                     * (varianceMatrix * xyCoordinatesMinusZmatrixTimesParameters);

  //I convert the TMatrixD into a SMatrix
  CTPPSPixelLocalTrack::CovarianceMatrix covarianceMatrix;
  for(unsigned int i=0; i<4; ++i){
    for(unsigned int j=0; j<4; ++j){
      covarianceMatrix[i][j] = parametersCovarianceMatrix[i][j];
    }
  }
  
  //I convert the TVectorD into a SVector
  CTPPSPixelLocalTrack::ParameterVector parameterVector;
  for(unsigned int i=0; i<4; ++i){
    parameterVector[i] = parameters[i];
  }

  CTPPSPixelLocalTrack goodTrack(z0_, parameterVector, covarianceMatrix, chiSquare);
  goodTrack.setValid(true);

  for(const auto & hit : pointList){
    CLHEP::Hep3Vector globalPoint = hit.globalPoint;
    math::GlobalPoint pointOnDet;
    bool foundPoint = calculatePointOnDetector(goodTrack, hit.detId, pointOnDet);
    if(!foundPoint){
      CTPPSPixelLocalTrack badTrack;
      badTrack.setValid(false);
      return badTrack;
    }
    double xResidual = globalPoint.x() - pointOnDet.x();
    double yResidual = globalPoint.y() - pointOnDet.y();
    LocalPoint residuals(xResidual,yResidual);

    TMatrixD globalError(hit.globalError);
    LocalPoint pulls(xResidual/std::sqrt(globalError[0][0]),yResidual/std::sqrt(globalError[1][1]));

    CTPPSPixelLocalTrack::CTPPSPixelFittedRecHit fittedRecHit(hit.recHit, pointOnDet, residuals, pulls);
    fittedRecHit.setIsUsedForFit(true);
    goodTrack.addHit(hit.detId, fittedRecHit);
  }

  return goodTrack;

}

//------------------------------------------------------------------------------------------------//

//The method calculates the hit pointed by the track on the detector plane
bool RPixPlaneCombinatoryTracking::calculatePointOnDetector(CTPPSPixelLocalTrack track, CTPPSPixelDetId planeId,
                                                            math::GlobalPoint &planeLineIntercept){
  double z0 = track.getZ0();
  CTPPSPixelLocalTrack::ParameterVector parameters = track.getParameterVector();


  TVectorD pointOnLine(0,2,parameters[0], parameters[1], z0,"END");
  math::GlobalVector tmpLineUnitVector = track.getDirectionVector();
  TVectorD lineUnitVector(0,2,tmpLineUnitVector.x(),tmpLineUnitVector.y(),tmpLineUnitVector.z(),"END");

  CLHEP::Hep3Vector tmpPointOnPlane = planePointMap_[planeId];
  TVectorD pointOnPlane(0,2,tmpPointOnPlane.x(), tmpPointOnPlane.y(), tmpPointOnPlane.z(),"END");
  TVectorD planeUnitVector(0,2,0.,0.,1.,"END");
  planeUnitVector = planeRotationMatrixMap_[planeId] * planeUnitVector;

  double denominator = (lineUnitVector*planeUnitVector);
  if(denominator==0){
    edm::LogError("RPixPlaneCombinatoryTracking")
      << "Error in RPixPlaneCombinatoryTracking::calculatePointOnDetector -> "
      << "Fitted line and plane are parallel. Removing this track";
    return false;
  }

  double distanceFromLinePoint = (pointOnPlane - pointOnLine)*planeUnitVector / denominator;

  TVectorD tmpPlaneLineIntercept = distanceFromLinePoint*lineUnitVector + pointOnLine;
  planeLineIntercept = math::GlobalPoint(tmpPlaneLineIntercept[0], tmpPlaneLineIntercept[1], tmpPlaneLineIntercept[2]);

  return true;

}
//------------------------------------------------------------------------------------------------//

// The method sorts the possible point combinations in order to process before fits on the highest possible number of points
std::vector<RPixPlaneCombinatoryTracking::PointAndReferencePair > 
  RPixPlaneCombinatoryTracking::orderCombinationsPerNumberOrPoints(
  PointAndReferenceMap inputMap){

  std::vector<PointAndReferencePair > sortedVector(inputMap.begin(),inputMap.end());
  std::sort(sortedVector.begin(),sortedVector.end(),functionForPlaneOrdering);

  return sortedVector;

}



