import {
  Chart,
  LineController,
  TimeScale,
  LinearScale,
  PointElement,
  LineElement,
  Legend,
  Filler,
  CategoryScale,
} from 'chart.js';
import 'chartjs-adapter-dayjs-4/dist/chartjs-adapter-dayjs-4.esm';
Chart.register(LineController);
Chart.register(TimeScale);
Chart.register(LinearScale);
Chart.register(CategoryScale);
Chart.register(PointElement);
Chart.register(LineElement);
Chart.register(Filler);
Chart.register(Legend);

import { ApiServiceContext, machine } from '../../services/ApiService.js';
import { useCallback, useEffect, useState, useContext } from 'preact/hooks';
import { computed } from '@preact/signals';
import { Spinner } from '../../components/Spinner.jsx';
import { parseHistoryData } from './utils.js';
import HistoryCard from './HistoryCard.jsx';

const connected = computed(() => machine.value.connected);

export function ShotHistory() {
  const apiService = useContext(ApiServiceContext);
  const [history, setHistory] = useState([]);
  const [loading, setLoading] = useState(true);
  const [loadingMore, setLoadingMore] = useState(false);
  const [hasMore, setHasMore] = useState(false);
  const [total, setTotal] = useState(0);
  const [offset, setOffset] = useState(0);
  const LIMIT = 5;

  const loadHistory = async (reset = false) => {
    const currentOffset = reset ? 0 : offset;
    
    if (reset) {
      setLoading(true);
      setHistory([]);
      setOffset(0);
    } else {
      setLoadingMore(true);
    }

    try {
      const response = await apiService.request({ 
        tp: 'req:history:list',
        offset: currentOffset,
        limit: LIMIT
      });
      
      const newHistory = response.history
        .map(parseHistoryData)
        .filter(e => !!e);
      
      if (reset) {
        setHistory(newHistory);
      } else {
        setHistory(prev => [...prev, ...newHistory]);
      }
      
      setTotal(response.total || 0);
      setHasMore(response.hasMore || false);
      setOffset(currentOffset + LIMIT);
      
    } catch (error) {
      console.error('Failed to load history:', error);
    } finally {
      setLoading(false);
      setLoadingMore(false);
    }
  };

  useEffect(() => {
    if (connected.value) {
      loadHistory(true);
    }
  }, [connected.value]);

  const onDelete = useCallback(
    async id => {
      setLoadingMore(true);
      try {
        await apiService.request({ tp: 'req:history:delete', id });
        // Reload from the beginning after deletion
        await loadHistory(true);
      } catch (error) {
        console.error('Failed to delete shot:', error);
        setLoadingMore(false);
      }
    },
    [apiService],
  );

  const loadMore = useCallback(() => {
    if (!loadingMore && hasMore) {
      loadHistory(false);
    }
  }, [loadingMore, hasMore, loadHistory]);

  if (loading) {
    return (
      <div className='flex w-full flex-row items-center justify-center py-16'>
        <Spinner size={8} />
      </div>
    );
  }

  return (
    <>
      <div className='mb-4 flex flex-row items-center gap-2'>
        <h2 className='flex-grow text-2xl font-bold sm:text-3xl'>Shot History</h2>
        <div className='text-sm text-gray-500'>
          Showing {history.length} of {total} shots
        </div>
      </div>

      <div className='grid grid-cols-1 gap-4 lg:grid-cols-12'>
        {history.map((item, idx) => (
          <HistoryCard shot={item} key={item.id || idx} onDelete={id => onDelete(id)} />
        ))}
        
        {history.length === 0 && !loading && (
          <div className='flex flex-row items-center justify-center py-20 lg:col-span-12'>
            <span>No shots available</span>
          </div>
        )}
        
        {hasMore && (
          <div className='flex flex-row items-center justify-center py-8 lg:col-span-12'>
            <button
              onClick={loadMore}
              disabled={loadingMore}
              className='btn btn-primary'
            >
              {loadingMore ? (
                <>
                  <Spinner size={4} />
                  Loading...
                </>
              ) : (
                `Load More (${total - history.length} remaining)`
              )}
            </button>
          </div>
        )}
        
        {loadingMore && !hasMore && (
          <div className='flex flex-row items-center justify-center py-8 lg:col-span-12'>
            <Spinner size={6} />
          </div>
        )}
      </div>
    </>
  );
}