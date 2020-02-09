>Exercise 6. Write a function to transmit a packet by checking that the next descriptor is free, copying the packet data into the next descriptor, and updating TDT. Make sure you handle the transmit queue being full.

# 代码

```
int
e1000_transmit(void *data, size_t len)
{
       uint32_t current = tdt->tdt;     //tail index in queue
       if(!(tx_desc_array[current].status & E1000_TXD_STAT_DD)) {
               return -E_TRANSMIT_RETRY;
       }
       tx_desc_array[current].length = len;
       tx_desc_array[current].status &= ~E1000_TXD_STAT_DD;
       tx_desc_array[current].cmd |= (E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS);
       memcpy(tx_buffer_array[current], data, len);            
       uint32_t next = (current + 1) % TXDESCS;
       tdt->tdt = next;
       return 0;
}
```
