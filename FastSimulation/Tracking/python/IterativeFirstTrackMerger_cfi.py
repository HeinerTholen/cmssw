import FWCore.ParameterSet.Config as cms

iterativeZeroTrackMerging = cms.EDFilter("FastTrackMerger",
    TrackProducers = cms.VInputTag(cms.InputTag("iterativeFirstTrackCandidatesWithTriplets"),
                                   cms.InputTag("iterativeFirstTracksWithTriplets")),
    trackAlgo = cms.untracked.uint32(4)
)


iterativeFirstTrackMerging = cms.EDFilter("FastTrackMerger",
    TrackProducers = cms.VInputTag(cms.InputTag("iterativeFirstTrackCandidatesWithPairs"),
                                   cms.InputTag("iterativeFirstTracksWithPairs")),
    trackAlgo = cms.untracked.uint32(5)
)


