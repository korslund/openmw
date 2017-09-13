#include "rechargeitems.hpp"

#include <components/esm/esmreader.hpp>
#include <components/esm/esmwriter.hpp>

namespace ESM
{


    void RechargeItems::write(ESMWriter &esm) const
    {
        for (std::vector<RechargeItem>::const_iterator it = mRechargeItems.begin(); it != mRechargeItems.end(); ++it)
        {
            RechargeItem item = *it;
            esm.writeHNString("KEY", item.key);
            esm.writeHNT("C", item.curCharge);
            esm.writeHNT("CMAX", item.maxCharge);
        }
    }

    void RechargeItems::load(ESMReader &esm)
    {
        std::vector<RechargeItem> tRechageItems;
        while (esm.isNextSub("ITEM"))
        {
            RechargeItem rItem;
            rItem.key = esm.getHNString("KEY");
            esm.getHNT(rItem.curCharge, "C");
            esm.getHNT(rItem.maxCharge, "CMAX");
            tRechageItems.push_back(rItem);
        }
    }

}
