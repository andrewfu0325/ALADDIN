#include <string>
#include <utility>

#include "aladdin/common/cacti-p/cacti_interface.h"
#include "aladdin/common/cacti-p/io.h"

#include "aladdin_tlb.hh"
#include "HybridDatapath.h"
#include "debug/HybridDatapath.hh"
/*hack, to fix*/
#define MIN_CACTI_SIZE 64

extern int DmaRead;
extern std::map<uint64_t, uint64_t> ReqLatency;

AladdinTLB::AladdinTLB(HybridDatapath* _datapath,
                       unsigned _num_entries,
                       unsigned _assoc,
                       Cycles _hit_latency,
                       Cycles _miss_latency,
                       Cycles _access_latency,
                       Addr _page_bytes,
                       unsigned _num_walks,
                       unsigned _bandwidth,
                       std::string _cacti_config,
                       std::string _accelerator_name)
    : datapath(_datapath), numEntries(_num_entries), assoc(_assoc),
      hitLatency(_hit_latency), missLatency(_miss_latency), accessLatency(_access_latency),
      pageBytes(_page_bytes),
      numOutStandingWalks(_num_walks), numOccupiedMissQueueEntries(0),
      cacti_cfg(_cacti_config), requests_this_cycle(0), bandwidth(_bandwidth), tlb_hits(0), tlb_misses(0) {
  if (numEntries > 0)
    tlbMemory = new TLBMemory(_num_entries, _assoc, _page_bytes);
  else
    tlbMemory = new InfiniteTLBMemory();
  regStats(_accelerator_name);
}

AladdinTLB::~AladdinTLB() { delete tlbMemory; }

void AladdinTLB::clear() {
  infiniteBackupTLB.clear();
  arrayLabelToVirtualAddrMap.clear();
  tlbMemory->clear();
}

void AladdinTLB::resetCounters() {
  reads = 0;
  updates = 0;
}

AladdinTLB::accessTLBEvent::accessTLBEvent(AladdinTLB* _tlb)
    : Event(Default_Pri, AutoDelete), tlb(_tlb){}

void AladdinTLB::accessTLBEvent::process() {
  DPRINTF(HybridDatapath, "Complete TLB access.\n");
  tlb->requests_this_cycle--;
}

const char* AladdinTLB::accessTLBEvent::description() const {
  return "TLB Access";
}

const std::string AladdinTLB::accessTLBEvent::name() const {
  return tlb->name() + ".access_tlb_event";
}

AladdinTLB::deHitQueueEvent::deHitQueueEvent(AladdinTLB* _tlb, bool _isDma)
    : Event(Default_Pri, AutoDelete), tlb(_tlb), isDma(_isDma){}

void AladdinTLB::deHitQueueEvent::process() {
  assert(!tlb->hitQueue.empty());
  if(isDma) {
    tlb->datapath->completeTLBRequestForDMA(tlb->hitQueue.front());
  } else {
    tlb->datapath->completeTLBRequest(tlb->hitQueue.front(), false);
  }
  tlb->hitQueue.pop_front();
}

const char* AladdinTLB::deHitQueueEvent::description() const {
  return "TLB Hit";
}

const std::string AladdinTLB::deHitQueueEvent::name() const {
  return tlb->name() + ".dehitqueue_event";
}

AladdinTLB::outStandingWalkReturnEvent::outStandingWalkReturnEvent(
    AladdinTLB* _tlb, bool _isDma, unsigned _pageBytes)
    : Event(Default_Pri, AutoDelete), tlb(_tlb), isDma(_isDma), pageBytes(_pageBytes){}

void AladdinTLB::outStandingWalkReturnEvent::process() {
  // TLB return events are free because only the CPU's hardware control units
  // can write to the TLB; programs can only read the TLB.
  assert(!tlb->missQueue.empty());
  Addr vpn = tlb->outStandingWalks.front();
  // Retrieve translation from the infinite backup storage if it exists;
  // otherwise, just use ppn=vpn.
  Addr ppn;
  if (tlb->infiniteBackupTLB.find(vpn) != tlb->infiniteBackupTLB.end()) {
    DPRINTF(HybridDatapath, "TLB miss was resolved in the backup TLB.\n");
    ppn = tlb->infiniteBackupTLB[vpn];
  } else {
    DPRINTF(HybridDatapath, "TLB miss was not resolved in the backup TLB.\n");
    ppn = vpn;
  }
  DPRINTF(HybridDatapath, "Translated vpn %#x -> ppn %#x.\n", vpn, ppn);
  tlb->insert(vpn, ppn);

  auto range = tlb->missQueue.equal_range(vpn);
  for (auto it = range.first; it != range.second; ++it) {
    if(isDma) {
      PacketPtr pkt = it->second;
      Addr page_offset = pkt->getAddr() % 4096;
      *(pkt->getPtr<Addr>()) = ppn + page_offset;
      tlb->datapath->completeTLBRequestForDMA(pkt);
    } else {
      tlb->datapath->completeTLBRequest(it->second, true);
    }
  }

  tlb->numOccupiedMissQueueEntries--;
  tlb->missQueue.erase(vpn);
  tlb->outStandingWalks.pop_front();
  tlb->updates++;  // Upon completion, increment TLB
}

const char* AladdinTLB::outStandingWalkReturnEvent::description() const {
  return "TLB Miss";
}

const std::string AladdinTLB::outStandingWalkReturnEvent::name() const {
  return tlb->name() + ".analytical_page_walk_event";
}

AladdinTLB::PageWalkEvent::PageWalkEvent(
  AladdinTLB *_tlb, MMUCache *_mmuCache, int _level, 
  MMUCache::PageTable* _pt, Addr _vaddr, bool _hasMMUCache) :
    tlb(_tlb), mmuCache(_mmuCache), level(_level), 
    pt(_pt), vaddr(_vaddr), hasMMUCache(_hasMMUCache) {}

void AladdinTLB::PageWalkEvent::process() {
  if(level != 0)  {
    PTIdx idx = (PTIdx)((vaddr & mask(48)) >> (39 - (4 - level) * 9)) & mask(9);
		Addr pt_paddr = pt->startAddr + (idx << 3);
    DPRINTF(HybridDatapath, "Level %d Page Table Addr: %#x for vaddr: %#x.\n", level, pt_paddr, vaddr);

    if(!mmuCache->isTagPresent(pt_paddr) || !hasMMUCache) {
      // MMU cache miss or not enable!
			size_t pte_size = 8;
			MMUCache::PageTable *npt;
      if(level == 1)
        npt = nullptr;
      else
        npt = mmuCache->walkPageTable(pt, idx);
			AladdinTLB::PageWalkEvent *page_walk_event = new AladdinTLB::PageWalkEvent(tlb, mmuCache, level - 1, npt, vaddr, hasMMUCache);
			DmaPort::DmaReqState *reqState = new DmaPort::DmaReqState(page_walk_event, pte_size, pt_paddr, 75 * 1000);

			uint8_t *data = new uint8_t[8];
			Request::Flags flags = Request::UNCACHEABLE;
			MemCmd::Command cmd = MemCmd::ReadFromDRAMReq;
			unsigned channel_idx = tlb->datapath->spadPort.addNewChannel(reqState);
			tlb->datapath->spadPort.dmaReqOnChannel(
					cmd, pt_paddr, pte_size, data, 0, reqState, flags, channel_idx);
			tlb->datapath->spadPort.sendDma();
    } else {
      // MMU cache hit!
			MMUCache::PageTable *npt = mmuCache->walkPageTable(pt, idx);
			AladdinTLB::PageWalkEvent *page_walk_event = new AladdinTLB::PageWalkEvent(tlb, mmuCache, level - 1, npt, vaddr, hasMMUCache);
      tlb->datapath->schedule(page_walk_event, tlb->datapath->clockEdge((Cycles)0));
    }
  } else {
		// TLB return events are free because only the CPU's hardware control units
		// can write to the TLB; programs can only read the TLB.
		assert(!tlb->missQueue.empty());
		Addr vpn = tlb->outStandingWalks.front();
		// Retrieve translation from the infinite backup storage if it exists;
		// otherwise, just use ppn=vpn.
		Addr ppn;
		if (tlb->infiniteBackupTLB.find(vpn) != tlb->infiniteBackupTLB.end()) {
			DPRINTF(HybridDatapath, "TLB miss was resolved in the backup TLB.\n");
			ppn = tlb->infiniteBackupTLB[vpn];
		} else {
			DPRINTF(HybridDatapath, "TLB miss was not resolved in the backup TLB.\n");
			ppn = vpn;
		}
    DPRINTF(HybridDatapath, "Final PPN: %#x for vaddr: %#x.\n", ppn, vaddr);
		DPRINTF(HybridDatapath, "Translated vpn %#x -> ppn %#x.\n", vpn, ppn);
		tlb->insert(vpn, ppn);

		auto range = tlb->missQueue.equal_range(vpn);
		for (auto it = range.first; it != range.second; ++it) {
			PacketPtr pkt = it->second;
			Addr page_offset = pkt->getAddr() % 4096;
			*(pkt->getPtr<Addr>()) = ppn + page_offset;
			tlb->datapath->completeTLBRequestForDMA(pkt);
		}

		tlb->numOccupiedMissQueueEntries--;
		tlb->missQueue.erase(vpn);
		tlb->outStandingWalks.pop_front();
		tlb->updates++;  // Upon completion, increment TLB
  }
}

const char* AladdinTLB::PageWalkEvent::description() const {
  return "Page Walker";
}

const std::string AladdinTLB::PageWalkEvent::name() const {
  return tlb->name() + ".page_walk_event";
}

std::pair<Addr, Addr> AladdinTLB::translateTraceToSimVirtual(PacketPtr pkt) {
  /* A somewhat complex translation process.
   *
   * First, we have to determine the simulation environment. If Aladdin is
   * being run standalone without a CPU model, then we directly use the trace
   * address to access memory. Otherwise, if Aladdin was invoked by a simulated
   * program, we perform address translation as follows:
   *
   * We use the node id to get the array name of the array being accessed. We
   * then look up the translation from this array name to the simulated
   * virtual address (which represents the head of the array). We also get the
   * base trace address from the same array name. Next, we compute
   * the offset between the accessed trace address and the base trace address
   * and add this offset to the base simulated virtual address.  Finally, we
   * consult the TLB to translate the simulated virtual address to the
   * simulated physical address.
   */
  Addr vaddr, vpn, page_offset;
  if (datapath->isExecuteStandalone()) {
    vaddr = pkt->req->getPaddr();
    page_offset = vaddr % pageBytes;
    vpn = vaddr - page_offset;
  } else {
    TLBSenderState* state = dynamic_cast<TLBSenderState*>(pkt->senderState);
    std::string array_name = datapath->getBaseAddressLabel(state->node_id);
    Addr cpu_base_sim_vaddr = lookupVirtualAddr(array_name);
    Addr acc_base_trace_addr =
        static_cast<Addr>(datapath->getBaseAddress(array_name));
    Addr acc_trace_req_vaddr = pkt->req->getPaddr();
    Addr array_offset;
    array_offset = acc_trace_req_vaddr - acc_base_trace_addr;
    DPRINTF(HybridDatapath,
            "Offset of trace acc address: %d\n", array_offset);

    // This is prticularly used for separate-buffer DMA operation
    ExecNode *node = datapath->exec_nodes[state->node_id];
    if(node->is_dma_op()) {
      DmaMemAccess* mem_access = node->get_dma_mem_access();
      DPRINTF(HybridDatapath,
              "Offset of trace cpu address: %d\n", node->is_dma_load()? mem_access->src_off : mem_access->dst_off);
      array_offset = node->is_dma_load()? mem_access->src_off : mem_access->dst_off;
    }
    /////////////////////////////////////////////////////////////

    vaddr = cpu_base_sim_vaddr + array_offset;
    DPRINTF(HybridDatapath,
            "Accessing array %s at node id %d with base address %#x.\n",
            array_name.c_str(),
            state->node_id,
            cpu_base_sim_vaddr);
    DPRINTF(HybridDatapath, "Translating vaddr %#x.\n", vaddr);
    page_offset = vaddr % pageBytes;
    vpn = vaddr - page_offset;
  }
  return std::make_pair(vpn, page_offset);
}

Addr AladdinTLB::translateInvisibly(Addr sim_vaddr) {
//bool AladdinTLB::translateInvisibly(PacketPtr pkt) {
  Addr vpn, ppn, page_offset, sim_paddr;
//  auto result = translateTraceToSimVirtual(pkt);
//  vpn = result.first;
//  page_offset = result.second;
  page_offset = sim_vaddr % pageBytes;
  vpn = sim_vaddr - page_offset;
  if (!tlbMemory->lookup(vpn, ppn)) {
    if (infiniteBackupTLB.find(vpn) != infiniteBackupTLB.end())
      ppn = infiniteBackupTLB[vpn];
    else
      ppn = vpn;
  }
  sim_paddr = ppn + page_offset;
  return sim_paddr;
//  *(pkt->getPtr<Addr>()) = ppn + page_offset;
//  DPRINTF(HybridDatapath, "Translated vpn %#x -> ppn %#x.\n", vpn, ppn);
//  return true;
}

bool AladdinTLB::translateTiming(PacketPtr pkt, bool isDma) {
  Addr vaddr, vpn, ppn, page_offset;
  int page_bits = pageBytes;
  if(!isDma) {
    auto result = translateTraceToSimVirtual(pkt);
    vpn = result.first;
    page_offset = result.second;
    vaddr = vpn + page_offset;
  } else {
    vaddr = pkt->getAddr();
    page_offset = vaddr % pageBytes;
    vpn = vaddr - page_offset;
  }

  if (isDma) {
    if(requests_this_cycle >= bandwidth) {
      DPRINTF(HybridDatapath, "Out of TLB bandwidth. Wait for next cycle.\n");
      return false;
    }
    accessTLBEvent *accessTLB = new accessTLBEvent(this);
    //datapath->schedule(accessTLB, datapath->clockEdge(accessLatency));
    datapath->schedule(accessTLB, datapath->clockEdge(accessLatency));
    requests_this_cycle++;
  }

  Addr paddr = infiniteBackupTLB[vpn] + page_offset;
  if(pkt->isRead() && ReqLatency.find(paddr) == ReqLatency.end()) {
    ReqLatency.insert(std::make_pair(paddr, curTick()));
    printf("Start DMA Req cycles 0x%x: %u\n", paddr, ReqLatency[paddr]);
    DmaRead++;
  }

  reads++;  // Both TLB hits and misses perform a read.
  if (tlbMemory->lookup(vpn, ppn)) {
    DPRINTF(HybridDatapath, "TLB hit(%s). Phys addr %#x.\n", 
            pkt->cmd.isRead()?"load":"store", ppn + page_offset);
    hits++;
    tlb_hits++;
    hitQueue.push_back(pkt);
    deHitQueueEvent* hq = new deHitQueueEvent(this, isDma);
    datapath->schedule(hq, datapath->clockEdge(hitLatency));
    // Due to the complexity of translating trace to virtual address, return
    // the complete address, not just the page number.
    *(pkt->getPtr<Addr>()) = ppn + page_offset;
    datapath->outstandingDmaReq++;
    return true;
  } else {

    if(datapath->hostPageWalk) {
			if (missQueue.find(vpn) == missQueue.end()) {
        AladdinTLB::TLBSenderStateForDma* state =
          dynamic_cast<AladdinTLB::TLBSenderStateForDma*>(pkt->senderState);
				if(state->trigger_page_walk_start == -1) {
					state->trigger_page_walk_start = curTick() / 1000;
				}
				if (numOutStandingWalks != 0 &&
						outStandingWalks.size() >= numOutStandingWalks) {
					DPRINTF(HybridDatapath,
							"Too many outstanding page walk, wait for next cycle\n");
					return false;
				}

				outStandingWalks.push_back(vpn);

				TheISA::TLB *cpu_dtb = datapath->getCPUDTBPtr();
				MMUCache *mmuCache = cpu_dtb->mmuCache;
				MMUCache::PageTable *L4PT = mmuCache->getTopLevelPageTable();

        // Page walker simulation //
        PageWalkEvent *page_walk_event = new PageWalkEvent(this, mmuCache, 4, L4PT, vaddr, true);
				datapath->schedule(page_walk_event, datapath->clockEdge(Cycles(30)));
        //////////////////////////

        // Page walker emulation //
/*				int delay(0);
        int memAccessLatency = missLatency / 4;
				vaddr &= mask(48);
				PTIdx L4Idx = (PTIdx)(vaddr >> 39);
				PTIdx L3Idx = (PTIdx)((vaddr >> 30) & mask(9));
				PTIdx L2Idx = (PTIdx)((vaddr >> 21) & mask(9));

				MMUCacheTag L3Tag = L4PT->startAddr + (L4Idx << 3);
				delay += (mmuCache->isTagPresent(L3Tag) ? 10 : (memAccessLatency + 10));
				MMUCache::PageTable *L3PT = mmuCache->walkPageTable(L4PT, L4Idx);

				MMUCacheTag L2Tag = L3PT->startAddr + (L3Idx << 3);
				delay += (mmuCache->isTagPresent(L2Tag) ? 4: memAccessLatency);
				MMUCache::PageTable *L2PT = mmuCache->walkPageTable(L3PT, L3Idx);

				MMUCacheTag L1Tag = L2PT->startAddr + (L2Idx << 3);
				delay += (mmuCache->isTagPresent(L1Tag)? 4 : memAccessLatency);
				mmuCache->walkPageTable(L2PT, L2Idx);

				// The last level page table needs to go through memeory
				delay += (memAccessLatency + 10);
				outStandingWalkReturnEvent* mq = new outStandingWalkReturnEvent(this, isDma, pageBytes);
        
				DPRINTF(HybridDatapath, "TLB Miss latency for host page walking: %d\n", delay);
				datapath->schedule(mq, curTick() + (Tick)(delay * 1000));*/
        ///////////////////////////



				numOccupiedMissQueueEntries++;
				DPRINTF(HybridDatapath,
						"Allocated TLB miss entry for addr %#x, page %#x\n",
						vaddr,
						vpn);

				tlb_misses++;
				misses++;
				missQueue.insert({ vpn, pkt });
				datapath->outstandingDmaReq++;
				return true;
			} else {
				DPRINTF(HybridDatapath,
						"Collapsed into existing miss entry for page %#x\n",
						vpn);
				return false; 
			}
    } else {
      // TLB miss! Let the TLB handle the walk, etc
      DPRINTF(HybridDatapath, "TLB miss(%s) for addr %#x\n", 
              pkt->cmd.isRead()?"load":"store", vaddr);

      if (missQueue.find(vpn) == missQueue.end()) {
        AladdinTLB::TLBSenderStateForDma* state =
          dynamic_cast<AladdinTLB::TLBSenderStateForDma*>(pkt->senderState);
        if(state->trigger_page_walk_start == -1) {
          state->trigger_page_walk_start = curTick() / 1000;
        }
        if (numOutStandingWalks != 0 &&
            outStandingWalks.size() >= numOutStandingWalks) {
          DPRINTF(HybridDatapath,
                  "Too many outstanding page walk, wait for next cycle\n");
          return false;
        }
        outStandingWalks.push_back(vpn);

        // Page walker simulation //
				TheISA::TLB *cpu_dtb = datapath->getCPUDTBPtr();
				MMUCache *mmuCache = cpu_dtb->mmuCache;
				MMUCache::PageTable *L4PT = mmuCache->getTopLevelPageTable();
        PageWalkEvent *page_walk_event = new PageWalkEvent(this, mmuCache, 4, L4PT, vaddr, false);
				datapath->schedule(page_walk_event, datapath->clockEdge(Cycles(10)));
        //////////////////////////
        // Page walker emulation //
//        outStandingWalkReturnEvent* mq = new outStandingWalkReturnEvent(this, isDma, pageBytes);
//				datapath->schedule(mq, curTick() + (Tick)(missLatency * 1000));
        ///////////////////////////

        numOccupiedMissQueueEntries++;
        DPRINTF(HybridDatapath,
                "Allocated TLB miss entry for addr %#x, page %#x\n",
                vaddr,
                vpn);

        tlb_misses++;
        misses++;
        missQueue.insert({ vpn, pkt });
        datapath->outstandingDmaReq++;
        return true;
      } else {
        //tlb_misses++;
        //misses++;
        DPRINTF(HybridDatapath,
                "Collapsed into existing miss entry for page %#x\n",
                vpn);
        return false; 
      }
    }
  } 
}

void AladdinTLB::insert(Addr vpn, Addr ppn) {
  tlbMemory->insert(vpn, ppn);
  infiniteBackupTLB[vpn] = ppn;
}

void AladdinTLB::insertBackupTLB(Addr vpn, Addr ppn) {
  infiniteBackupTLB[vpn] = ppn;
}
/*Always return true if TLB has unlimited bandwidth (bandwidth == 0). Otherwise,
 * we check how many requests have been served so far and how many outstanding
 * page table lookups. */
bool AladdinTLB::canRequestTranslation() {
  if (bandwidth == 0)
    return true;
  return (requests_this_cycle < bandwidth &&
          numOccupiedMissQueueEntries < numOutStandingWalks);
}

void AladdinTLB::regStats(std::string accelerator_name) {
  using namespace Stats;
  hits.name("system." + accelerator_name + ".tlb.hits").desc("TLB hits").flags(
      total | nonan);
  misses.name("system." + accelerator_name + ".tlb.misses")
      .desc("TLB misses")
      .flags(total | nonan);
  hitRate.name("system." + accelerator_name + ".tlb.hitRate")
      .desc("TLB hit rate")
      .flags(total | nonan);
  hitRate = hits / (hits + misses);
  reads.name("system." + accelerator_name + ".tlb.reads")
      .desc("TLB reads")
      .flags(total | nonan);
  updates.name("system." + accelerator_name + ".tlb.updates")
      .desc("TLB updates")
      .flags(total | nonan);
}

void AladdinTLB::computeCactiResults() {
  DPRINTF(HybridDatapath, "Invoking CACTI for TLB power and area estimates.\n");
  uca_org_t cacti_result = cacti_interface(cacti_cfg);
  if (numEntries >= MIN_CACTI_SIZE) {
    readEnergy = cacti_result.power.readOp.dynamic * 1e12;
    writeEnergy = cacti_result.power.writeOp.dynamic * 1e12;
    leakagePower = cacti_result.power.readOp.leakage * 1000;
    area = cacti_result.area;
  } else {
    /*Assuming it scales linearly with cache size*/
    readEnergy =
        cacti_result.power.readOp.dynamic * 1e12 * numEntries / MIN_CACTI_SIZE;
    writeEnergy =
        cacti_result.power.writeOp.dynamic * 1e12 * numEntries / MIN_CACTI_SIZE;
    leakagePower =
        cacti_result.power.readOp.leakage * 1000 * numEntries / MIN_CACTI_SIZE;
    area = cacti_result.area * numEntries / MIN_CACTI_SIZE;
  }
}

void AladdinTLB::getAveragePower(unsigned int cycles,
                                 unsigned int cycleTime,
                                 float* avg_power,
                                 float* avg_dynamic,
                                 float* avg_leak) {
  *avg_dynamic = (reads.value() * readEnergy + updates.value() * writeEnergy) /
                 (cycles * cycleTime);
  *avg_leak = leakagePower;
  *avg_power = *avg_dynamic + *avg_leak;
}

std::string AladdinTLB::name() const { return datapath->name() + ".tlb"; }

bool TLBMemory::lookup(Addr vpn, Addr& ppn, bool set_mru) {
  int way = (vpn / pageBytes) % ways;
  for (int i = 0; i < sets; i++) {
    if (entries[way][i].vpn == vpn && !entries[way][i].free) {
      ppn = entries[way][i].ppn;
      assert(entries[way][i].mruTick > 0);
      if (set_mru) {
        entries[way][i].setMRU();
      }
      entries[way][i].hits++;
      return true;
    }
  }
  return false;
}

void TLBMemory::insert(Addr vpn, Addr ppn) {

  Addr a;
  if (lookup(vpn, a)) {
    return;
  }
  int way = (vpn / pageBytes) % ways;
  AladdinTLBEntry* entry = nullptr;
  Tick minTick = curTick();
  for (int i = 0; i < sets; i++) {
    if (entries[way][i].free) {
      entry = &entries[way][i];
      break;
    } else if (entries[way][i].mruTick <= minTick) {
      minTick = entries[way][i].mruTick;
      entry = &entries[way][i];
    }
  }
  assert(entry);
  if (!entry->free) {
    DPRINTF(HybridDatapath, "Evicting entry for vpn %#x\n", entry->vpn);
  }

  entry->vpn = vpn;
  entry->ppn = ppn;
  entry->free = false;
  entry->setMRU();
}
