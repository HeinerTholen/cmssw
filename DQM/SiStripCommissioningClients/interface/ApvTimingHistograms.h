#ifndef DQM_SiStripCommissioningClients_ApvTimingHistograms_H
#define DQM_SiStripCommissioningClients_ApvTimingHistograms_H

#include "DQM/SiStripCommissioningClients/interface/CommissioningHistograms.h"
#include "DQM/SiStripCommissioningSummary/interface/ApvTimingSummaryFactory.h"
#include "DQM/SiStripCommissioningAnalysis/interface/ApvTimingAnalysis.h"

class MonitorUserInterface;
class SiStripSummary;

class ApvTimingHistograms : public CommissioningHistograms {

 public:
  
  ApvTimingHistograms( MonitorUserInterface* );
  virtual ~ApvTimingHistograms();

  typedef SummaryHistogramFactory<ApvTimingAnalysis::Monitorables> Factory;
  
  /** */
  void histoAnalysis();
  /** */
  void createSummaryHisto( const sistrip::SummaryHisto&, 
			   const sistrip::SummaryType&, 
			   const std::string& directory );

 private:

  std::map<uint32_t,ApvTimingAnalysis::Monitorables> data_;

  std::auto_ptr<Factory> factory_;
  
};

#endif // DQM_SiStripCommissioningClients_ApvTimingHistograms_H

