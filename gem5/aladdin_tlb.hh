#ifndef ALADDIN_TLB_HH
#define ALADDIN_TLB_HH

#include <map>
#include <set>
#include <deque>
#include <unordered_map>
#include <utility>

/* Hard-coded the real ISA implementation 
   to get tlb lookup function */
#include "arch/x86/tlb.hh"
#include "arch/generic/mmu_cache.hh"
///////////////////////////////
#include "base/statistics.hh"
#include "cpu/thread_context.hh"
#include "dev/dma_device.hh"
#include "mem/mem_object.hh"
#include "mem/request.hh"

class HybridDatapath;

class AladdinTLBEntry {
 public:
  Addr vpn;
  Addr ppn;
  bool free;
  Tick mruTick;
  uint32_t hits;

  AladdinTLBEntry() : vpn(0), ppn(0), free(true), mruTick(0), hits(0) {}
  AladdinTLBEntry(AladdinTLBEntry& entry)
      : vpn(entry.vpn), ppn(entry.ppn), free(entry.free),
        mruTick(entry.mruTick), hits(entry.hits) {}

  void setMRU() { mruTick = curTick(); }
  void clear() {
    vpn = 0;
    ppn = 0;
    free = true;
    mruTick = 0;
    hits = 0;
  }
};

class BaseTLBMemory {

 public:
  virtual ~BaseTLBMemory() {}
  virtual void clear() = 0;
  virtual bool lookup(Addr vpn, Addr& ppn, bool set_mru = true) = 0;
  /* Inserts a translation into the TLB.
   */
  virtual void insert(Addr vpn, Addr ppn) = 0;
  // Name of the TLB structure for printing traces.
};

class TLBMemory : public BaseTLBMemory {
  int numEntries;
  int sets;
  int ways;
  int pageBytes;

  AladdinTLBEntry** entries;

 protected:
  TLBMemory() {}

 public:
  TLBMemory(int _numEntries, int associativity, int _pageBytes)
      : numEntries(_numEntries), sets(associativity), pageBytes(_pageBytes) {
    if (sets == 0) {
      sets = numEntries;
    }
    assert(numEntries % sets == 0);
    ways = numEntries / sets;
    entries = new AladdinTLBEntry* [ways];
    for (int i = 0; i < ways; i++) {
      entries[i] = new AladdinTLBEntry[sets];
    }
  }
  ~TLBMemory() {
    for (int i = 0; i < ways; i++) {
      delete[] entries[i];
    }
    delete[] entries;
  }

  virtual void clear() {
    for (int i = 0; i < ways; i++) {
      entries[i]->clear();
    }
  }

  virtual bool lookup(Addr vpn, Addr& ppn, bool set_mru = true);
  virtual void insert(Addr vpn, Addr ppn);
};

class InfiniteTLBMemory : public BaseTLBMemory {
  std::map<Addr, Addr> entries;

 public:
  InfiniteTLBMemory() {}
  ~InfiniteTLBMemory() {}

  virtual bool lookup(Addr vpn, Addr& ppn, bool set_mru = true) {
    auto it = entries.find(vpn);
    if (it != entries.end()) {
      ppn = it->second;
      return true;
    } else {
      ppn = Addr(0);
      return false;
    }
  }

  virtual void insert(Addr vpn, Addr ppn) { entries[vpn] = ppn; }

  virtual void clear() { entries.clear(); }
};

class AladdinTLB {
 private:
  HybridDatapath* datapath;
  void regStats(std::string accelerator_name);

  unsigned numEntries;
  unsigned assoc;
  Cycles hitLatency;
  Cycles missLatency;
  Cycles accessLatency;
  Addr pageBytes;
  unsigned numOutStandingWalks;
  unsigned numOccupiedMissQueueEntries;
  std::string cacti_cfg;  // CACTI 6.5+ config file.

  // Power from CACTI.
  float readEnergy;
  float writeEnergy;
  float leakagePower;
  float area;

  BaseTLBMemory* tlbMemory;

  class accessTLBEvent : public Event {
   public:
    /*Constructs a deHitQueueEvent*/
    accessTLBEvent(AladdinTLB* _tlb);
    /*Processes the event*/
    void process();
    /*Returns the description of this event*/
    const char* description() const;
    /* Returns name of this event. */
    virtual const std::string name() const;

   private:
    /* The pointer the to AladdinTLB unit*/
    AladdinTLB* tlb;
  };

  class deHitQueueEvent : public Event {
   public:
    /*Constructs a deHitQueueEvent*/
    deHitQueueEvent(AladdinTLB* _tlb, bool _isDma);
    /*Processes the event*/
    void process();
    /*Returns the description of this event*/
    const char* description() const;
    /* Returns name of this event. */
    virtual const std::string name() const;

   private:
    /* The pointer the to AladdinTLB unit*/
    AladdinTLB* tlb;
    bool isDma;
  };

  class outStandingWalkReturnEvent : public Event {
   public:
    /*Constructs a outStandingWalkReturnEvent*/
    outStandingWalkReturnEvent(AladdinTLB* _tlb, bool _isDma, unsigned _pageBytes);
    /*Processes the event*/
    void process();
    /*Returns the description of this event*/
    const char* description() const;
    /* Returns name of this event. */
    virtual const std::string name() const;

   private:
    /* The pointer to the AladdinTLB unit*/
    AladdinTLB* tlb;
    bool isDma;
    unsigned pageBytes;
  };

  class PageWalkEvent : public Event {
   public:
    /*Constructs a outStandingWalkReturnEvent*/
    PageWalkEvent(AladdinTLB *_tlb, MMUCache *_mmuCache, int _level, 
                  MMUCache::PageTable* _pg, Addr _vaddr, bool _hasMMUCache);
    /*Processes the event*/
    void process();
    /*Returns the description of this event*/
    const char* description() const;
    /* Returns name of this event. */
    virtual const std::string name() const;

   private:
    /* The pointer to the AladdinTLB unit*/
    AladdinTLB *tlb;
    MMUCache *mmuCache;
    int level;
    MMUCache::PageTable *pt;
    Addr vaddr;
    bool hasMMUCache;
  };

  std::deque<PacketPtr> hitQueue;
  std::deque<Addr> outStandingWalks;
  std::unordered_multimap<Addr, PacketPtr> missQueue;

  /* Stores all explicitly mapped TLB translations. Later, we should move this
   * functionality into gem5's page table model. For now, we'll keep them
   * separate and just model the timing.
   */
  std::map<Addr, Addr> infiniteBackupTLB;

  /* Translations from array labels to simulated virtual addresses.
   *
   * This is purely an artifact of how Aladdin is implemented so it doesn't
   * need to be modeled for timing.
   */
  std::map<std::string, Addr> arrayLabelToVirtualAddrMap;

 public:
  AladdinTLB(HybridDatapath* _datapath,
             unsigned _num_entries,
             unsigned _assoc,
             Cycles _hit_latency,
             Cycles _miss_latency,
             Cycles _access_latency,
             Addr pageBytes,
             unsigned _num_walks,
             unsigned _bandwidth,
             std::string _cacti_config,
             std::string _accelerator_name);
  ~AladdinTLB();

  std::string name() const;
  void clear();
  void resetCounters();

  /* Getters. */
  unsigned getNumEntries() { return numEntries; }
  unsigned getAssoc() { return assoc; }
  Cycles getHitLatency() { return hitLatency; }
  Cycles getMissLatency() { return missLatency; }
  Addr getPageBytes() { return pageBytes; }
  Addr pageMask() {
    return pageBytes - 1;
  };
  /* A TLB is perfect is its missLatency is zero. */
  bool getIsPerfectTLB() { return missLatency == 0 ? true : false; }
  unsigned getNumOutStandingWalks() { return numOutStandingWalks; }

  /* Inserts a translation between simulated virtual and physical pages.
   *
   * The simulated virtual pages are NOT obtained from the trace - this is the
   * simulated virtual address space of the simulator.
   *
   * Note that the entries are actually page-aligned addresses instead of just
   * page numbers (e.g. 0xf000 instead of 0xf).
   */
  void insert(Addr vpn, Addr ppn);

  /* Inserts a translation between simulated virtual and physical pages to the
   * infiniteBackupTLB, not the real TLB.
   *
   * This operation only happens when we activate accelerators, to dump the
   * true virtual to physical translation from CPU to accelerators.
   */
  void insertBackupTLB(Addr vpn, Addr ppn);

  /* Translates array labels to simulated virtual addresses.
   *
   * Because trace addresses and the simulated addresses live in entirely
   * disjunct address spaces, we need more than just bit manipulations to
   * properly translate trace address to physical addresses.
   * This method inserts a mapping between the array name and its
   * corresponding simulated virtual address. */

  void insertArrayLabelToVirtual(std::string array_label, Addr sim_vaddr) {
    arrayLabelToVirtualAddrMap[array_label] = sim_vaddr;
  }

  /* Get the simulated virtual address from its array name. */
  Addr lookupVirtualAddr(std::string array_label) {
    return arrayLabelToVirtualAddrMap[array_label];
  }

  /* Perform a TLB translation with timing. */
  bool translateTiming(PacketPtr pkt, bool isDma = false);

  /* Perform a TLB translation invisibly.
   *
   * Invisibly means it takes zero time and has zero side effects - no
   * incrementing of statistics, no changes to TLB state. This is required
   * due to the current constraints of the DMA system.
   */
  //bool translateInvisibly(PacketPtr pkt);
  Addr translateInvisibly(Addr sim_vaddr);

  /* Translate a trace virtual address to a simulated virtual address. */
  std::pair<Addr, Addr> translateTraceToSimVirtual(PacketPtr pkt);

  bool canRequestTranslation();
  void incrementRequestCounter() { requests_this_cycle++; }
  void resetRequestCounter() { requests_this_cycle = 0; }

  /* Power and area calculations. */
  void computeCactiResults();
  void getAveragePower(unsigned int cycles,
                       unsigned int cycleTime,
                       float* avg_power,
                       float* avg_dynamic,
                       float* avg_leak);
  float getArea() { return area; }

  class TLBSenderState : public Packet::SenderState {
   public:
    TLBSenderState(unsigned _node_id) : node_id(_node_id) {}
    unsigned node_id;
  };

  class TLBSenderStateForDma : public Packet::SenderState {
   public:
    TLBSenderStateForDma(unsigned _node_id, unsigned _channel_idx, 
                         DmaPort::DmaReqState *_dmaReqState, Addr _acc_taddr) 
        : node_id(_node_id), channel_idx(_channel_idx), 
          dmaReqState(_dmaReqState), acc_taddr(_acc_taddr), translation_start(-1), trigger_page_walk_start(-1) {}
    unsigned node_id;
    unsigned channel_idx;
    DmaPort::DmaReqState *dmaReqState;
    Addr acc_taddr;
    Tick translation_start;
    Tick trigger_page_walk_start;
  };

  /* Number of TLB translation requests in the current cycle. */
  unsigned requests_this_cycle;
  /* Maximum translation requests per cycle. Zero if there is unlimited
   * bandwidth. */
  unsigned bandwidth;

  Stats::Scalar hits;
  Stats::Scalar misses;
  Stats::Scalar reads;
  Stats::Scalar updates;
  Stats::Formula hitRate;

  unsigned tlb_hits;
  unsigned tlb_misses;

  friend class HybridDatapath;
};

#endif
